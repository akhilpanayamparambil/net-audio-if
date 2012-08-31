/*
 * isocket.h
 *
 *  Created on: 2009/10/09
 *      Author: novi
 */

#ifndef ISOCKET_H_
#define ISOCKET_H_
#include "inet.h"


#define WIZProtocolTCP 1
#define WIZProtocolUDP 2
#define WIZProtocolIPRAW 3

extern uint8_t WIZSocketCreate(int sock, uint8_t protocol, uint16_t srcPort, uint8_t flag);
extern uint8_t WIZListen(int sock);
extern uint8_t WIZConnect(int sock, const uint8_t* dstAddr, uint16_t dstPort);
extern uint8_t WIZClose(int sock);
extern uint8_t WIZDisconnect(int sock);
extern uint16_t WIZSend(int sock, const uint8_t* buf, uint16_t len);
extern uint16_t WIZRecv(int sock, uint8_t* buf, uint16_t len);
extern uint16_t WIZSendTo(int sock, const uint8_t* buf, uint16_t len, const uint8_t* dstAddr, uint16_t dstPort);
extern uint16_t WIZRecvFrom(int sock, uint8_t* buf, uint16_t len, uint8_t* srcAddr, uint16_t* srcPort);

extern uint8_t WIZGetSocketStatus(int sock);

/*
#define WIZInterruptIR 0x1
#define WIZInterruptSR 0x2

extern void WIZInterruptProc(void);
extern void WIZInterruptCallback(int sock, uint8_t changed);
extern void WIZInterruptRegisterForSocket(int sock, void (*callback)(int sock));

*/

#endif /* ISOCKET_H_ */
