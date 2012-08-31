/*
 * net.c
 *
 *  Created on: 2009/10/13
 *      Author: novi
 */

#include "isocket.h"
#include "monitor.h"
#include "taskmgr.h"
#include "play.h"
#include "Oled.h"

#define NetSockCmd	1
#define NetSockMusic 0

#define NetSockMusicLocalPort 5000
#define NetSockMusicSrvPort 5000

#define NetSockCmdLocalPort 6000
#define NetSockCmdSrvPort 6000

static uint8_t dstip[] = {10, 0, 1, 4};

static uint8_t g_netTaskEnable = 0, g_musicEnable = 0;
static uint16_t g_musicPacketSize = 512;

UINT _net_music_stream(uint8_t* buf, UINT size, UINT* rs);


void _printfIPandPort(uint8_t* ip, uint16_t p){
	xprintf("%d.%d.%d.%d:%d\n", ip[0], ip[1], ip[2], ip[3], p);
}

void NetInit(void)
{
	// Create socket
	WIZSocketCreate(NetSockMusic, WIZProtocolUDP, NetSockMusicLocalPort, 0);
	WIZSocketCreate(NetSockCmd, WIZProtocolTCP, NetSockCmdLocalPort, 0);

	// Set Music dst port

	WIZSendTo(NetSockMusic, NULL, 0, dstip, NetSockMusicSrvPort);
}

void NetTaskStart(void)
{
	g_netTaskEnable = 1;
}

void NetParseCmd(uint8_t* cmd, uint16_t size)
{
//	xprintf("cmd:%s\n", cmd);
	xprintf("command received, %d bytes, type:0x%X, subtype:0x%X\n", size, cmd[0],cmd[1]);

	if (cmd[0] == 1) {
		// OLED Type
		if (cmd[1] == 1) {
			OLEDSetRectX(cmd[2], cmd[4]);
			OLEDSetRectY(cmd[3], cmd[5]);
		} else if (cmd[1] == 2) {
			int i;
			OLEDWriteCommand(OLEDCMDMemoryWrite);
			OLEDWriteDataStart();
			for (i = 0; i < size-2; i+=3) {
				OLEDColor color;
				color.a = cmd[i+2];
				color.b = cmd[i+3];
				color.c = cmd[i+4];
				OLEDWritePixel(color);
			}
			OLEDWriteEnd();
		} else if (cmd[1] == 3) {
			if (cmd[2] == 1) {
				OLEDVccON();
			} else {
				OLEDVccOFF();
			}
		}

	} else if (cmd[0] == 2) {
		// VS Type
		if (cmd[1] == 1) {
			g_musicEnable = 0;
			VSPlayStop();
		} else if (cmd[1] == 2) {
			g_musicEnable = 1;
			VSPlay(_net_music_stream);
		} else if (cmd[1] == 3) {
			VSSetVolumeControl(cmd[2]);
			VSSetVolume(cmd[3]);
		} else if (cmd[1] == 4) {
			VSReset();
		}

	} else if (cmd[0] == 3) {
		// HW Type
	}
}



void NetTask(void)
{
	while (g_netTaskEnable == 0) {
		Sleep(100);
	}
	_wiz_prepare_fifo_test();
	NetInit();
	for (;;) {
		//xprintf("nettask\n");
		Sleep(100);

		// Check current status for command socket
		if (WIZGetSocketStatus(NetSockCmd) == WIZSRSOCK_CLOSED ||
				WIZGetSocketStatus(NetSockCmd) == WIZSRSOCK_INIT) {
			// Not connected or established
			WIZConnect(NetSockCmd, dstip, NetSockCmdSrvPort);
			xprintf("not connected, trying connect...\n");
			_printfIPandPort(dstip, NetSockCmdSrvPort);
			Sleep(1000);
			continue;
		}

		if (WIZGetSocketStatus(NetSockCmd) == WIZSRSOCK_CLOSE_WAIT) {
			WIZClose(NetSockCmd);
			xprintf("disconnected by server, now closing...\n");
			continue;
		}

		if (WIZGetSocketStatus(NetSockCmd) != WIZSRSOCK_ESTABLISHED) {
			xprintf("connecting...(0x%X)\n", WIZGetSocketStatus(NetSockCmd));
			continue;
		}


		uint8_t buf[1024];
		// Connection established
		uint16_t recvsize = WIZRecv(NetSockCmd, buf, 1024);
		if (recvsize == 0) {
			continue;
		}

		NetParseCmd(buf, recvsize);
	}
}

UINT _net_music_stream(uint8_t* buf, UINT size, UINT* rs)
{
	//xprintf("_net music stream 0x%X\n",buf);
	int emptyflg = 0;
	while(1) {
		if (g_musicEnable == 0) {
			return 0;
		}
		uint16_t curSize = _wiz_read2(k_sockToBaseAddr[NetSockMusic]+WIZSxRXRSR);
		if (curSize > 3500) {
			xprintf("RX buffer will full\n");
			WIZSend(NetSockMusic, (uint8_t*)"FULL", 5);
		}
		if (curSize !=0 && curSize >= (g_musicPacketSize+8)) {
			emptyflg = 0;
			break;
		}
		if (curSize == 0) {
			emptyflg++;
		//xprintf("RX buffer empty, %d\n", emptyflg);
		//	WIZSend(NetSockMusic, (uint8_t*)"EMPTY", 6);
		}

		Sleep(10);
	}
		*rs = WIZRecv(NetSockMusic, buf, size);
		WIZSend(NetSockMusic, (uint8_t*)"ACK", 4);

		//xprintf("data get %d byte\n", *rs);

		if (*rs < g_musicPacketSize) {
			xprintf("data get error, %d byte\n", *rs);
			WIZSend(NetSockMusic, (uint8_t*)"ERROR", 6);
//			return 0;
			uint16_t i;
			for (i = *rs; i < g_musicPacketSize; i++) {
				buf[i] = 0;
			}
			*rs = g_musicPacketSize;
		}

	return *rs;
}
