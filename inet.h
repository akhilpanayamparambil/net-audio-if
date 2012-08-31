/*
 * inet.h
 *
 *  Created on: 2009/10/06
 *      Author: novi
 */

#ifndef INET_H_
#define INET_H_

#include <string.h>
#include <inttypes.h>


extern void WIZInit(void);
extern void WIZPower(uint8_t on);

extern void WIZSetMAC(const uint8_t* mac);
extern void WIZSetIPMaskGw(const uint8_t* ip, const uint8_t* mask, const uint8_t* dgw);
extern void WIZSetIP(const uint8_t* ip);

void _wiz_prepare_fifo_test(void);
void _wiz_test_send(int count);

uint16_t _wiz_send_data(int sock, const uint8_t* srcAddr, uint16_t size);
uint16_t _wiz_recv_data(int sock, uint8_t* dstAddr, uint16_t size);
uint16_t _wiz_recv_data_udp(int sock, uint8_t* dstAddr, uint16_t size, uint8_t* srcIP, uint16_t* srcPort);

void _wiz_write(uint16_t addr, uint8_t data);
void _wiz_write2(uint16_t addr, uint16_t data);
void _wiz_write_buf(uint16_t addr, const uint8_t* data, uint16_t len);

uint8_t _wiz_read(uint16_t addr);
uint16_t _wiz_read2(uint16_t addr);
void _wiz_read_buf(uint16_t addr, uint8_t* data, uint16_t len);

#define WIZMR 0x0000
#define WIZGAR 0x0001
#define WIZSUBR 0x0005
#define WIZSHAR 0x0009
#define WIZSIPR 0x000f
#define WIZIR	0x0015
#define WIZIMR	0x0016
#define WIZRTR	0x0017
#define WIZRCR 0x0019
#define WIZRMSR 0x001a
#define WIZTMSR 0x001b
#define WIZUIPR 0x002a
#define	WIZUPORT 0x002e

#define WIZTXMEMBASE 0x4000
#define WIZRXMEMBASE 0x6000

#define WIZS0BASE 0x0400
#define WIZS1BASE 0x0500
#define WIZS2BASE 0x0600
#define WIZS3BASE 0x0700

#define WIZSxMR 0
#define WIZSxCR 0x1
#define WIZSxIR 0x2
#define WIZSxSR 0x3
#define WIZSxPORT 0x4
#define WIZSxDHAR 0x6
#define WIZSxDIPR 0xc
#define WIZSxDPORT 0x10
#define WIZSxMSSR 0x12
#define WIZSxPROTO 0x14
#define WIZSxTOS 0x15
#define WIZSxTTL 0x16


#define WIZSxTXFSR 0x20
#define WIZSxTXRD 0x22
#define WIZSxTXWR 0x24
#define WIZSxRXRSR 0x26
#define WIZSxRXRD 0x28

#define WIZMRCLOSED 0
#define WIZMRTCP 0x1
#define WIZMRUDP 0x2
#define WIZMRIPRAW 0x3
#define WIZMRMACRAW 0x4
#define WIZMRPPPoE	0x5
#define WIZMRMULTI 0x80
#define WIZMRNDMC 0x20

#define WIZIRSEND_OK (1 << 4)
#define WIZIRTIMEOUT (1 << 3)
#define WIZIRRECV (1 << 2)
#define WIZIRDISCON (1 << 1)
#define WIZIRCON (1)

#define WIZSRSOCK_CLOSED 0
#define WIZSRSOCK_INIT 0x13
#define WIZSRSOCK_LISTEN 0x14
#define WIZSRSOCK_ESTABLISHED 0x17
#define WIZSRSOCK_CLOSE_WAIT 0x1c
#define WIZSRSOCK_UDP 0x22
#define WIZSRSOCK_IPRAW 0x32
#define WIZSRSOCK_MACRAW 0x42
#define WIZSRSOCK_PPPOE 0x5f

#define WIZCROPEN 0x1
#define WIZCRLISTEN 0x2
#define WIZCRCONNECT 0x4
#define WIZCRDISCON 0x8
#define WIZCRCLOSE 0x10
#define WIZCRSEND 0x20
#define WIZCRSEND_MAC 0x21
#define WIZCRSEND_KEEP 0x22
#define WIZCRRECV 0x40

extern const uint16_t k_sockToBaseAddr[];
#define SOCKMAX 4

#endif /* INET_H_ */
