/*
 * isocket.c
 *
 *  Created on: 2009/10/09
 *      Author: novi
 */


#include "isocket.h"

#define WIZWaitForCRCommand(sock) {while(_wiz_read(k_sockToBaseAddr[(sock)]+WIZSxCR));}

static uint8_t g_WIZCurrentProtocol[SOCKMAX] = {0, 0, 0, 0};
/*static uint8_t g_WIZLatestSR[SOCKMAX] = {0, 0, 0, 0};
static uint8_t g_WIZLatestIR[SOCKMAX] = {0, 0, 0, 0};

static void (*g_WIZCallback)(int, uint8_t)[SOCKMAX];// = {0, 0, 0, 0};
*/
uint8_t WIZSocketCreate(int sock, uint8_t protocol, uint16_t srcPort, uint8_t flag)
{
	// Set mode (TCP, UDP, IPRAW) and flags
	_wiz_write(k_sockToBaseAddr[sock]+WIZSxMR, flag|protocol);
	g_WIZCurrentProtocol[sock] = protocol;

	// Set source port
	_wiz_write2(k_sockToBaseAddr[sock]+WIZSxPORT, srcPort);

	// Set open command
	_wiz_write(k_sockToBaseAddr[sock]+WIZSxCR, WIZCROPEN);
	WIZWaitForCRCommand(sock);

	/*
	if (_wiz_read(k_sockToBaseAddr[sock]+WIZSxSR) != WIZSRSOCK_INIT) {
		// If open failed
		_wiz_write(k_sockToBaseAddr[sock]+WIZSxCR, WIZCRCLOSE);
		WIZWaitForCRCommand(sock);
		return -1;
	}
*/
	return sock;
}

uint8_t WIZListen(int sock)
{
	if (_wiz_read(k_sockToBaseAddr[sock]+WIZSxSR) == WIZSRSOCK_INIT) {
		// Set Listen
		_wiz_write(k_sockToBaseAddr[sock]+WIZSxCR, WIZCRLISTEN);
		WIZWaitForCRCommand(sock);
		return 0;
	}
	return -1;
}
uint8_t WIZConnect(int sock, const uint8_t* dstAddr, uint16_t dstPort)
{
	uint8_t i;
	// Set dst ip address
	for (i = 0; i < 4; ++i) {
		_wiz_write(k_sockToBaseAddr[sock]+WIZSxDIPR+i, dstAddr[i]);
	}

	// Set dst port
	_wiz_write2(k_sockToBaseAddr[sock]+WIZSxDPORT, dstPort);

	// Set connect commnad
	_wiz_write(k_sockToBaseAddr[sock]+WIZSxCR, WIZCRCONNECT);
	WIZWaitForCRCommand(sock);

	return 0;
}

uint8_t WIZClose(int sock)
{
	_wiz_write(k_sockToBaseAddr[sock]+WIZSxCR, WIZCRCLOSE);
	WIZWaitForCRCommand(sock);
	return 0;
}

uint8_t WIZDisconnect(int sock)
{
	_wiz_write(k_sockToBaseAddr[sock]+WIZSxCR, WIZCRDISCON);
	WIZWaitForCRCommand(sock);
	return 0;
}

uint8_t WIZGetSocketStatus(int sock)
{
	return _wiz_read(k_sockToBaseAddr[sock]+WIZSxSR);
}

uint16_t WIZSend(int sock, const uint8_t* buf, uint16_t len)
{
	return _wiz_send_data(sock, buf, len);
}

uint16_t WIZRecv(int sock, uint8_t* buf, uint16_t len)
{
	if (g_WIZCurrentProtocol[sock] == WIZProtocolTCP) {
		return _wiz_recv_data(sock, buf, len);
	} else if (g_WIZCurrentProtocol[sock] == WIZProtocolUDP) {
		return _wiz_recv_data_udp(sock, buf, len, NULL, NULL);
	} else if (g_WIZCurrentProtocol[sock] == WIZProtocolIPRAW) {
		return 0; // Not implemented
	}

	return 0;
}

uint16_t WIZSendTo(int sock, const uint8_t* buf, uint16_t len, const uint8_t* dstAddr, uint16_t dstPort)
{
	if (dstAddr) {
		_wiz_write_buf(k_sockToBaseAddr[sock]+WIZSxDIPR, dstAddr, 4);
		_wiz_write2(k_sockToBaseAddr[sock]+WIZSxDPORT, dstPort);
	}
	return _wiz_send_data(sock, buf, len);
}

uint16_t WIZRecvFrom(int sock, uint8_t* buf, uint16_t len, uint8_t* srcAddr, uint16_t* srcPort)
{
	return _wiz_recv_data_udp(sock, buf, len, srcAddr, srcPort);
}

/*
void WIZInterruptProc(void)
{
	uint8_t i;
	for (i = 0; i < SOCKMAX; i++) {
		if (! g_WIZCallback[i]) {
			continue;
		}
		uint8_t changed = 0;
		uint8_t curSR = WIZGetSocketStatus(i);
		if (g_WIZLatestSR[i] != curSR) {
			// Interrupt
			g_WIZLatestSR[i] = curSR;
			changed |= WIZInterruptSR;
		}
		uint8_t curIR = _wiz_read(k_sockToBaseAddr[i]+WIZSxIR);
		if (g_WIZLatestIR[i] != curIR) {
			g_WIZLatestIR[i] = curIR;
			changed |= WIZInterruptIR;
		}
		if (changed) {
			(*g_WIZCallback[i])(i, changed);
		}
	}
}


void WIZInterruptRegisterForSocket(int sock, void (*callback)(int sock))
{
	g_WIZCallback[sock] = callback;
}*/
