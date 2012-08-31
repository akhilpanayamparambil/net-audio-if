/*----------------------------------------------------------------------*/
/* CQ_LPC2388: Test Program (Audio player)                              */
/*----------------------------------------------------------------------*/

#include <string.h>
#include "LPC2300.h"
#include "integer.h"
#include "interrupt.h"
#include "comm.h"
#include "ff.h"
#include "monitor.h"
#include "diskio.h"
#include "taskmgr.h"
#include "Oled.h"
#include "inet.h"

void pb_timerproc (void);
void VSPlayerTask(void);
void monitor (void);
void NetTask(void);

void dummy(void){

	for(;;) {
		Sleep(100);
	}
}


FATFS Fatfs[_DRIVES];		/* File system object for each logical drive */
FIL File1, File2;			/* File objects */
DIR Dir;					/* Directory object */
BYTE Buff[16384] __attribute__ ((aligned (4))) ;		/* Working buffer */

volatile UINT Timer;		/* 1kHz increment timer */


#define SS0 256
#define SS1 256
DWORD TaskStk0[SS0], TaskStk1[SS1];
DWORD TaskStk2[SS1];
DWORD TaskContext[3][16];

void Delay(volatile unsigned long nCount)
{
	for(; nCount != 0; nCount--);
}


/*---------------------------------------------------------*/
/* 1kHz Interval Timer                                     */
/*---------------------------------------------------------*/

void Isr_TIMER0 (void)
{
	static BYTE div;
	WORD w;
	int n;


	T0IR = 1;	/* Clear irq flag */

	Timer++;

	for (n = 0; n < 3; n++) {
		w = TaskContext[n][1];
		if (w != 0xFFFF && w-- != 0) TaskContext[n][1] = w;
	}

	switch (++div & 3) {
	case 0:
		disk_timerproc();
		break;
	case 1:
		pb_timerproc();
		break;
	case 2:
	case 3:
		break;
	}
}



/*---------------------------------------------------------*/
/* User Provided RTC Function for FatFs module             */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support an RTC.                     */
/* This function is not required in read-only cfg.         */

DWORD get_fattime ()
{
	/* Pack date and time into a DWORD variable (No RTC, use 2009.04.01 00:07:00) */
	return	  ((DWORD)(2009 - 1980) << 25)
			| ((DWORD)4 << 21)
			| ((DWORD)1 << 16)
			| ((DWORD)0 << 11)
			| ((DWORD)7 << 5)
			| ((DWORD)0 >> 1);
}


/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */


static
void IoInit (void)
{
#define PLL_N		2UL
#define PLL_M		72UL
#define CCLK_DIV	4

	if ( PLLSTAT & (1 << 25) ) {
		PLLCON = 1;				/* Disconnect PLL output if PLL is in use */
		PLLFEED = 0xAA;
		PLLFEED = 0x55;
	}
	PLLCON = 0;				/* Disable PLL */
	PLLFEED = 0xAA;
	PLLFEED = 0x55;
	CLKSRCSEL = 0;			/* Select IRC (4MHz) as the PLL clock input */

	PLLCFG = ((PLL_N - 1) << 16) | (PLL_M - 1);	/* Re-configure PLL */
	PLLFEED = 0xAA;
	PLLFEED = 0x55;
	PLLCON = 1;				/* Enable PLL */
	PLLFEED = 0xAA;
	PLLFEED = 0x55;

	while ((PLLSTAT & (1 << 26)) == 0);	/* Wait for PLL locked */

	CCLKCFG = CCLK_DIV-1;	/* Select CCLK frequency (divide ratio of hclk) */
	PLLCON = 3;				/* Connect PLL output to the sysclk */
	PLLFEED = 0xAA;
	PLLFEED = 0x55;

	MAMCR = 0;				/* Configure MAM for 72MHz operation */
	MAMTIM = 3;
	MAMCR = 2;

	PCLKSEL0 = 0x00000000;	/* Select peripheral clock */
	PCLKSEL1 = 0x00000000;

	ClearVector();

	SCS |= 1;				/* Enable FIO0 and FIO1 */

	FIO1PIN2 = 0x04;		/* -|-|-|-|-|LED|-|- */
	FIO1DIR2 = 0x04;
	PINMODE3 = 0x00000020;

	/* Initialize Timer0 as 1kHz interval timer */
	RegisterVector(TIMER0_INT, Isr_TIMER0, PRI_LOWEST, CLASS_IRQ);
	T0CTCR = 0;
	T0MR0 = 18000 - 1;	/* 18M / 1k = 18000 */
	T0MCR = 0x3;		/* Clear TC and Interrupt on MR0 match */
	T0TCR = 1;

	IrqEnable();

	uart0_init();
}

static void OLedPortInit(void)
{
	FIO4DIR |= 0x1fff; // P4: OLED
	FIO4MASK = 0;
	OLEDVccOFF();
	OLEDRESET_ENABLE();

	Delay(0xffff);

//	OLEDVccON();
	OLEDRESET_DISABLE();

//	OLEDWriteCommandStart();

	OLEDWriteCommand(OLEDCMDDisplayOFF); // Set sleep mode on
	OLEDWriteOption(0xb3, 0xf1); // Clock
	OLEDWriteOption(0xca, 0x7f); // Multiplex rate
	OLEDWriteOption(0xa0, 0xb4); // Re-Map, Color depth
	//OLEDWriteOption(0xab, 0x01); // Function selection
	//OLEDWriteOption(0xb5, 0x00); // GPIO
	OLEDWriteOptions3(0xb4, 0xa0, 0xb5, 0x55); // Set Segment low voltage
	OLEDWriteOptions3(0xc1, 0xc8, 0x80, 0xc8); // Set Contrast current
	OLEDWriteOption(0xc7, 0x0f); // Set Master current control
	OLEDWriteOption(0xb1, 0x32); // Set Phese length
	OLEDWriteOptions3(0xb2, 0xa4, 0x00, 0x00); // Enhance Driving Scheme Capability
	OLEDWriteOption(0xbb, 0x17); // Set Pre-charge voltage
	OLEDWriteOption(0xb6, 0x01); // Set Second Pre-charge period
	OLEDWriteOption(0xbe, 0x05); // Set VCOMH voltage
	OLEDWriteOption(OLEDCMDSetDisplayStartLine, 32);
	//OLEDWriteOption(OLEDCMDSetDisplayOffset, 0);


		OLEDWriteCommand(OLEDCMDDisplayON);

		OLEDSetRectX(0, 127);
		OLEDSetRectY(0, 127);

			OLEDWriteCommand(0x5c);
			OLEDWriteDataStart();
			int i;
			for(i = 0; i < 128*128*OLEDColorSize; i++) {
				OLEDWrite(0x0);
			}
			OLEDWriteEnd();
}

static void PMInit(void)
{

	// P4[31:30] BSTAT2, BSTAT1 (Input, pull-up)
	// P4[25] BUSBPG (Input, pull-up)
	FIO4DIR &= 0x3dffffff;

	// P4[24] BPWSEL (Output)
	FIO4DIR |= 0x01000000;
	FIO4MASK3 = 0;
	//FIO4CLR3 = 0x1;
	FIO4SET3 = 0x1;

}


int main (void)
{
	IoInit();
	OLedPortInit();
	PMInit();
	WIZInit();

	StartTask(TaskContext[0], VSPlayerTask,  &TaskStk0[SS0]);
	StartTask(TaskContext[1], monitor, &TaskStk1[SS1]);
	StartTask(TaskContext[2], NetTask, &TaskStk2[SS1]);

	for (;;) {
		DispatchTask(TaskContext[0]);
		DispatchTask(TaskContext[1]);
		DispatchTask(TaskContext[2]);
	}

	for(;;);
}


