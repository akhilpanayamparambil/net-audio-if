
#include "play.h"
#include <string.h>
#include "comm.h"
#include "interrupt.h"
#include "monitor.h"
#include "ff.h"
#include "taskmgr.h"
#include "LPC2300.h"

/* LPC2388 pin mappings
P2[9] - XRESET#
P2[8] - DREQ
P0[5] - XDCS#
P0[6] - XCS#
P0[7] - SCLK
P0[9] - SI
P0[8] - SO
P0[23] - Volume AD.0
P1[26] - SW4 Rev
P1[27] - SW3 Stop
P4[28] - SW2 Play
P4[29] - SW1 Fwd
*/

#define XRESET_LOW()	FIO2CLR1 = 0x02
#define XRESET_HIGH()	FIO2SET1 = 0x02
#define DREQ			(FIO2PIN1 & 1)
#define XDCS_LOW()		FIO0CLR0 = 0x20
#define XDCS_HIGH()		FIO0SET0 = 0x20
#define XCS_LOW()		FIO0CLR0 = 0x40
#define XCS_HIGH()		FIO0SET0 = 0x40

#define SSP1BSY			(SSP1SR & 0x10)
#define SSP1TNF			(SSP1SR & 0x02)
#define SSP1RNE			(SSP1SR & 0x04)
#define PUT_SSP1(val)	SSP1DR=(val); while (SSP1BSY)

#define	DSP_IRQ_EN()	IO2_INT_EN_R = 0x100
#define	DSP_IRQ_DIS()	IO2_INT_EN_R = 0x000



/* Transmission FIFO */
static BYTE Fifo[16384] __attribute__ ((section(".netram")));			/* Buffer (must be multiple of 1024) */
static volatile UINT ofs_r, ofs_w, remain;


static volatile uint8_t g_VSVolume, g_VSTmrVol, g_VSVolChanged, g_VSEnabled, g_VSVolSrc;

static UINT (*g_playCallback)(uint8_t* buf, UINT requiresize, UINT* gotsize);



static
void ISR_intDSP (void)
{
	UINT rptr, cnt;
	BYTE d;


	IO2_INT_CLR = 0xFFFFFFFF;

	if (g_VSVolChanged) {
		g_VSVolChanged = 0;
		d = g_VSVolume ? (127 - g_VSVolume / 2) : 254;
		//xprintf("vol=%d\n", d);
		XCS_LOW();
		XDCS_HIGH();
		PUT_SSP1(2);
		PUT_SSP1(11);
		PUT_SSP1(d);
		PUT_SSP1(d);
		XCS_HIGH();
	}

	cnt = remain;
	if (cnt > 0 && DREQ) {
		rptr = ofs_r;
		XCS_HIGH();
		XDCS_LOW();
		do {
			d = Fifo[rptr];
			rptr = (rptr + 1) % sizeof(Fifo);
			while (!SSP1TNF);
			SSP1DR = d;
		} while (--cnt > 0 && DREQ);
		while (SSP1BSY);
		XDCS_HIGH();
		ofs_r = rptr;
		remain = cnt;
	}
}



void pb_timerproc (void)
{
	static BYTE div5;
	INT ad, v;


	if (g_VSVolSrc == VSVolumeControlAD && g_VSEnabled && ++div5 >= 5) {
		div5 = 0;
		/* Volume control */
		ad = (AD0GDR & 0xFFFF) / 256;
		AD0CR = 0x01201101;			/* 0000_0_001_00_1_0_000_0_00010010_01000000 */
		if (g_VSTmrVol) {
			g_VSTmrVol--;
		} else {
			v = g_VSVolume;
			if (v < ad - 1) {
				g_VSVolume = (BYTE)(ad - 1);
				g_VSVolChanged = 1;
			}
			if (v > ad + 1) {
				g_VSVolume = (BYTE)(ad + 1);
				g_VSVolChanged = 1;
			}
		}
	}
}


void _vs_write(uint8_t addr, uint16_t data)
{
	while(!DREQ);
	XCS_LOW();
	XDCS_HIGH();
	PUT_SSP1(2);
	PUT_SSP1(addr);
	PUT_SSP1((uint8_t)(data >> 8));
	PUT_SSP1((uint8_t)data);
	XCS_HIGH();
	while(!DREQ);
}




uint16_t _vs_read(uint8_t addr)
{
	uint16_t dat;


	while(!DREQ);
	XCS_LOW();
	PUT_SSP1(3);
	PUT_SSP1(addr);

	// Flush Read FIFO
	volatile uint16_t dummy;
	while(SSP1RNE) { dummy = SSP1DR;}

	PUT_SSP1(0);
	dat = SSP1DR; dat <<= 8;
	PUT_SSP1(0);
	dat |= SSP1DR;
	XCS_HIGH();
	while(!DREQ);
	return dat;
}



uint8_t VSPlayRaw(const char* fname)
{
	FIL fil;
	if (f_open(&fil, fname, FA_READ|FA_OPEN_EXISTING) != FR_OK) return -1;

	char buf[1024];

	XCS_HIGH(); XDCS_LOW();

	UINT readsize;
	while (f_read(&fil, buf, 1024, &readsize) == FR_OK) {
		int i;
		for (i = 0; i < readsize; i++) {
			PUT_SSP1(buf[i]);
			while(!DREQ);
		}
	}
	XDCS_HIGH();

	return 0;
}

void VSPlay(UINT (*callback)(uint8_t* buf, UINT requiresize, UINT* gotsize))
{
	g_playCallback = callback;

	// Start timer
	g_VSEnabled	= 1;
}

void VSReset(void)
{
	_vs_write(0x0, 0x0804); // Software reset
	_vs_write(0x3, 0xc000); // Clock setting

	_vs_write(0x7, 0xc017);
	_vs_write(0x6, 0x00f0); // GPIO

	_vs_write(0x7, 0xc040);
	_vs_write(0x6, 0x000c); // I2S 48Khz
}

void VSPlayStop(void)
{
	// Stop timer
	g_VSEnabled = 0;

	// Clear callback function
	g_playCallback = NULL;

}

void VSSetVolume(uint8_t v)
{
	if (g_VSVolSrc == VSVolumeControlExt) {
		g_VSVolume = v;
		g_VSVolChanged = 1;
	}
}

void VSSetVolumeControl(uint8_t vc)
{
	g_VSVolSrc = vc;
}


void _vs_play_with_callback(void)
{
	if (! g_playCallback) {
		return;
	}

	// Flush FIFO
	//memset(&Fifo[0], 0, 4096);

	// Stop current decode
	//_vs_write(0x0, 0x0808);

	ofs_r = 0, ofs_w = 0, remain = 0; // Flush FIFO
	UINT br;
	DSP_IRQ_EN();		/* Enable VS1011 interrupt */
	while (1) {
		Sleep(0);

		/* Store audio data into FIFO when there is >=1024 byte space in the FIFO */
		if (remain <= sizeof(Fifo) - 1024) {
			// will stop
			if (g_playCallback == NULL) {
				xprintf("callback broken\n");
				break;
			}
			if ((*g_playCallback)(&Fifo[ofs_w], 1024, &br) == 0) {
				xprintf("callback return null\n");
				break;
			}
			//xprintf("ofs_w %d, br %d\n", ofs_w, br);
			ofs_w = (ofs_w + br) % sizeof(Fifo);
			DSP_IRQ_DIS();
			remain += br;
			if (DREQ) ISR_intDSP();
			DSP_IRQ_EN();
		}
	}

	DSP_IRQ_DIS();
	g_playCallback = NULL;
}




static
void VSInit(void)
{
	PCLKSEL0 = (PCLKSEL0 & 0xFFCFFFFF) | 0x00200000;	/* Set PCLK_SSP1 CCLK/2 = 36MHz */
		SSP1CPSR = 0x02;		/* Set clock divisor (CPSR=2) */
		SSP1CR0 = 0x0507;		/* Set frame format (SCR=5, SPI0, 8bit) */
		SSP1CR1 = 0x02;			/* Enable SSP1 (SSE=1) */

		FIO2DIR1 |= 0x02;		/* P2[9] = Output */
		PINMODE4 = (PINMODE4 & 0xFFFCFFFF) | 0x00030000;	/* Set pull-down on P2[8] pin */
		FIO0PINL |= 0x0060;		/* P0[9:5]=[00011] */
		FIO0DIRL |= 0x01E0;		/* P0[9:5]=[ioooo] */
		PINSEL0 =  (PINSEL0  & 0xFFF03FFF) | 0x000A8000;	/* Attach SSP1 to P0[9:7] */

		PCONP |= (1 << 12);									/* ADC power on */
		PINMODE1 = (PINMODE1 & 0xFFFF3FFF) | 0x00008000;	/* Disable pull-up on P0[23] */
		PINSEL1 =  (PINSEL1  & 0xFFFF3FFF) | 0x00004000;	/* Attach AD0.0 to P0[23] */

		RegisterVector(EINT3_INT, ISR_intDSP, PRI_LOWEST, CLASS_IRQ);	/* Register ISR */

		g_VSEnabled = 1;

		XRESET_LOW();
		Sleep(100);
		XRESET_HIGH();
		while(!DREQ);
		g_VSVolChanged = 1;

		VSReset();

		g_VSVolSrc = VSVolumeControlAD;
}

void VSPlayerTask (void *arg)
{

	VSInit();

	for (;;) {
		Sleep(10);
		_vs_play_with_callback();
	}
}



