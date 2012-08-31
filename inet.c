/*
 * inet.c
 *
 *  Created on: 2009/10/06
 *      Author: novi
 */

#include "inet.h"
#include "LPC2300.h"

void xprintf (const char*, ...);
void Delay(unsigned long);

#define NET_PWR_ON() {FIO3CLR3 = 0x04;}
#define NET_PWR_OFF() {FIO3SET3 = 0x04;}
#define NET_CS_ENABLE() {FIO0CLR2 = 0x01;}
#define NET_CS_DISABLE() {FIO0SET2 = 0x01;}
#define NET_RST_ENABLE() {FIO1CLR2 = 0x08;}
#define NET_RST_DISABLE() {FIO1SET2 = 0x08;}

#define SSP0BSY			(SSP0SR & 0x10)
#define SSP0TNF			(SSP0SR & 0x02)
#define SSP0RNE			(SSP0SR & 0x04)
#define WRITE_SSP0(val)	{ SSP0DR=(val); while (SSP0BSY); }

static uint16_t g_RXBASE[SOCKMAX];
static uint16_t g_RXMASK[SOCKMAX];
static uint16_t g_TXBASE[SOCKMAX];
static uint16_t g_TXMASK[SOCKMAX];

const uint16_t k_sockToBaseAddr[] = {WIZS0BASE, WIZS1BASE, WIZS2BASE, WIZS3BASE};



void WIZInit(void)
{
	PINSEL1 = (PINSEL1 & 0xffffffc0) | 0x00000028; // Attach SSP1 to P0[16], [17]
	PINSEL0 = (PINSEL0 & 0x3fffffff) | 0x80000000; // Attach SSP1 to P0[15] SCK

	// Set output
	FIO1DIR |= 0x00080000; // RST P1[19]
	FIO3DIR |= 0x04000000; // PWR P3[26]
	FIO0DIR |= 0x00010000; // CS  P0[16]
	FIO0DIR |= 0x00008000; // SCK P0[15]
	FIO0DIR |= 0x00040000; // MOSI P0[18]

	// Set input
	FIO0DIR &= 0xfffdffff; // MISO

	PINMODE0 = (PINMODE0 & 0xFFFFfff3) | 0x00000008; // not pull-up or down
	PINMODE7 = (PINMODE7 & 0xFFCFffff) | 0x00200000; // PWR

		PCLKSEL1 = (PCLKSEL1 & 0xFFFFf3ff) | 0x00000400;	/* Set PCLK_SSP0 CCLK = 72MHz */
		SSP0CPSR = 0x02;		/* Set clock divisor (CPSR=2) */
		SSP0CR0 = 0x030f;		/* Set frame format (SCR=3, SPI0, 16bit) */
		SSP0CR1 = 0x02;			/* Enable SSP0 (SSE=1) */

		NET_PWR_OFF();
		NET_RST_ENABLE();
}

void WIZPower(uint8_t on)
{
	if (on) {
		NET_PWR_ON();
		Delay(0xffff);
		NET_RST_ENABLE();
		Delay(0xffff);
		NET_RST_DISABLE();
		Delay(0xffff);
		_wiz_write(0, 0x80);
	} else {
		NET_PWR_OFF();
		NET_RST_ENABLE();
	}
}

void _wiz_write(uint16_t addr, uint8_t data)
{
	NET_CS_ENABLE();
	/*WRITE_SSP0(0xf0);
	WRITE_SSP0(addr >> 8);
	WRITE_SSP0((uint8_t)addr);

	xprintf("addr = %0X\n", addr >> 8);
	xprintf("addr = %0X\n", (uint8_t)addr);
	xprintf("data = %0X\n", (uint8_t)data);
*/
//	WRITE_SSP0(data);

	WRITE_SSP0(0xf000|(addr >> 8));
	WRITE_SSP0((addr << 8)|data);

	//xprintf("d1 = %0X\n", 0xf000|(addr >> 8));
	//xprintf("d2 = %0X\n", (addr << 8)|data);

	NET_CS_DISABLE();
}


uint8_t _wiz_read(uint16_t addr)
{
	NET_CS_ENABLE();
/*	WRITE_SSP0(0x0f);
	WRITE_SSP0(addr >> 8);
	WRITE_SSP0((uint8_t)addr);

	// Flush Read FIFO
	volatile uint8_t dummy;
	while(SSP0RNE) { dummy = SSP0DR;}

	WRITE_SSP0(0xff);
	uint8_t data = SSP0DR;
	*/
	volatile uint16_t dummy;
	while(SSP0RNE) { dummy = SSP0DR;}

	WRITE_SSP0(0x0f00|(addr >> 8));
	dummy = SSP0DR;
	WRITE_SSP0((addr << 8)| 0xff);
	uint16_t data = SSP0DR;

	//xprintf("d1 = %0X\n", dummy);
	//	xprintf("d2 = %0X\n", data);

	NET_CS_DISABLE();

	return (uint8_t)data;
}


void _wiz_write2(uint16_t addr, uint16_t data)
{
	_wiz_write(addr, data >> 8);
	_wiz_write(addr+1, (uint8_t)data);
}

uint16_t _wiz_read2(uint16_t addr)
{
	uint16_t data = _wiz_read(addr);
	data <<= 8;
	data |= _wiz_read(addr+1);
	return data;
}

void _wiz_write_buf(uint16_t addr, const uint8_t* data, uint16_t len)
{
	uint16_t i;
	for (i = 0; i < len; i++) {
		_wiz_write(addr+i, data[i]);
	}
}

void _wiz_read_buf(uint16_t addr, uint8_t* data, uint16_t len)
{
	uint16_t i;
	for (i = 0; i < len; i++) {
		data[i] = _wiz_read(addr+i);
	}
}

uint16_t _wiz_send_data(int sock, const uint8_t* srcAddr, uint16_t size)
{
	if (!size) return 0;

	uint16_t freeSize = _wiz_read2(k_sockToBaseAddr[sock]+WIZSxTXFSR);
	if (freeSize < size) {
		return 0;
	}

	uint16_t offset = _wiz_read2(k_sockToBaseAddr[sock]+WIZSxTXWR) & g_TXMASK[sock];
	uint16_t startAddr = g_TXBASE[sock] + offset;

	// If over flow Tx memory
	if ((offset + size) > (g_TXMASK[sock] + 1)) {
		uint16_t upperSize = (g_TXMASK[sock] + 1) - offset;
		_wiz_write_buf(startAddr, srcAddr, upperSize);
		uint16_t leftSize = size - upperSize;
		_wiz_write_buf(g_TXBASE[sock], srcAddr+upperSize, leftSize);
	} else {
		_wiz_write_buf(startAddr, srcAddr, size);
	}

	// Increase TXWR
	uint16_t txwr = _wiz_read2(k_sockToBaseAddr[sock]+WIZSxTXWR);
	_wiz_write2(k_sockToBaseAddr[sock]+WIZSxTXWR, txwr+size);

	// Set SEND
	_wiz_write(k_sockToBaseAddr[sock]+WIZSxCR, WIZCRSEND);

	return size;
}

uint16_t _wiz_recv_data(int sock, uint8_t* dstAddr, uint16_t size)
{
	if (!size) return 0;

	uint16_t curSize = _wiz_read2(k_sockToBaseAddr[sock]+WIZSxRXRSR);
	if (curSize < size) {
		size = curSize;
	}
	uint16_t offset = _wiz_read2(k_sockToBaseAddr[sock]+WIZSxRXRD) & g_RXMASK[sock];
	uint16_t startAddr = g_RXBASE[sock] + offset;

	// If overflow Rx memory
	if ((offset + size) > (g_RXMASK[sock] + 1)) {
		uint16_t upperSize = (g_RXMASK[sock] + 1) - offset;
		_wiz_read_buf(startAddr, dstAddr, upperSize);
		uint16_t leftSize = size - upperSize;
		_wiz_read_buf(g_RXBASE[sock], dstAddr+upperSize, leftSize);
	} else {
		_wiz_read_buf(startAddr, dstAddr, size);
	}

	// Increase RxRD
	uint16_t rxwd = _wiz_read2(k_sockToBaseAddr[sock]+WIZSxRXRD);
	_wiz_write2(k_sockToBaseAddr[sock]+WIZSxRXRD, rxwd+size);

	// Set RECV
	_wiz_write(k_sockToBaseAddr[sock]+WIZSxCR, WIZCRRECV);
	return size;
}


uint16_t _wiz_recv_data_udp(int sock, uint8_t* dstAddr, uint16_t size, uint8_t* srcIP, uint16_t* srcPort)
{
	uint8_t bufhead[8];
	uint16_t recvsize = _wiz_recv_data(sock, bufhead, 8);
	if (recvsize != 8) {
		xprintf("head recv error!\n");
		return 0;
	}

	if(srcIP) {
		// Check header
		uint8_t i;
		for (i = 0; i < 4; i++) {
			srcIP[i] = bufhead[i];
		}
	}

	if (srcPort) {
		uint16_t dstport = bufhead[4];
		dstport <<= 8;
		dstport |= bufhead[5];
		*srcPort = dstport;
	}

	uint16_t packsize = bufhead[6];
	packsize <<= 8;
	packsize |= bufhead[7];

	uint16_t getsize = _wiz_recv_data(sock, dstAddr, packsize);
	if (packsize != getsize) {
		xprintf("packet error, expect %d but read %d byte\n", packsize, getsize);
		return getsize;
	}

	return getsize;
}

void WIZSetMAC(const uint8_t* mac)
{
	_wiz_write_buf(WIZSHAR, mac, 6);
}
void WIZSetIPMaskGw(const uint8_t* ip, const uint8_t* mask, const uint8_t* dgw)
{
	_wiz_write_buf(WIZSIPR, ip, 4);
	_wiz_write_buf(WIZSUBR, mask, 4);
	_wiz_write_buf(WIZGAR, dgw, 4);
}

void WIZSetIP(const uint8_t* ip)
{
	const uint8_t mask[] = {255, 255, 255, 0};
	uint8_t gw[4];
	gw[0] = ip[0];
	gw[1] = ip[1];
	gw[2] = ip[2];
	gw[3] = 1;

	WIZSetIPMaskGw(ip, mask, gw);
}


void _wiz_prepare_fifo_test(void)
{
	// TX, RX: ?, ?, ?, 4k
	_wiz_write(WIZRMSR, 0x06);
	_wiz_write(WIZTMSR, 0x06);

	g_RXBASE[0] = WIZRXMEMBASE;
	g_RXMASK[0] = 4096 - 1;

	g_TXBASE[0] = WIZTXMEMBASE;
	g_TXMASK[0] = 4096 - 1;

	g_RXBASE[1] = g_RXBASE[0] + g_RXMASK[0] + 1;
	g_RXMASK[1] = 2048 - 1;

	g_TXBASE[1] = g_TXBASE[0] + g_TXMASK[0] + 1;
	g_TXMASK[1] = 2048 - 1;

}

void _wiz_test_send(int count)
{
	unsigned char buf[1900];
	int i;
	for (i = 0; i < 1900; i++) {
		buf[i] = i%255;
	}

	for (i = 0; i < count; i++) {
		_wiz_send_data(0, buf, 1900);
	}
}


