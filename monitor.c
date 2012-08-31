/*---------------------------------------------------------------------*/
/* LPC2388 test program: Monitor                                       */

#include <stdarg.h>
#include <string.h>
#include "LPC2300.h"
#include "diskio.h"
#include "ff.h"
#include "monitor.h"
#include "taskmgr.h"
#include "Oled.h"
#include "play.h"
#include "isocket.h"


int packetsize = 128;

void (*xfunc_out)(char);

extern FATFS Fatfs[];
extern BYTE Buff[16384];
extern FIL File1, File2;			/* File objects */
extern DIR Dir;						/* Directory object */
extern volatile UINT Timer;


DWORD acc_size;				/* Work register for fs command */
WORD acc_files, acc_dirs;
FILINFO Finfo;
#if _USE_LFN
char Lfname[512];
#endif

char Linebuf[256];			/* Console input buffer */


char xgetc_n (void)
{
	char c;

	c = 0;
	if (uart0_test())
		c = uart0_get();

	return c;
}



char xgetc (void)
{
	char c;

	do {
		Sleep(0);
		c = xgetc_n();
	} while (!c);
	return c;
}



int xatoi (char **str, long *res)
{
	DWORD val;
	BYTE c, radix, s = 0;


	while ((c = **str) == ' ') (*str)++;
	if (c == '-') {
		s = 1;
		c = *(++(*str));
	}
	if (c == '0') {
		c = *(++(*str));
		if (c <= ' ') {
			*res = 0; return 1;
		}
		if (c == 'x') {
			radix = 16;
			c = *(++(*str));
		} else {
			if (c == 'b') {
				radix = 2;
				c = *(++(*str));
			} else {
				if ((c >= '0')&&(c <= '9'))
					radix = 8;
				else
					return 0;
			}
		}
	} else {
		if ((c < '1')||(c > '9'))
			return 0;
		radix = 10;
	}
	val = 0;
	while (c > ' ') {
		if (c >= 'a') c -= 0x20;
		c -= '0';
		if (c >= 17) {
			c -= 7;
			if (c <= 9) return 0;
		}
		if (c >= radix) return 0;
		val = val * radix + c;
		c = *(++(*str));
	}
	if (s) val = -val;
	*res = val;
	return 1;
}




void xputc (char c)
{
	if (c == '\n') xfunc_out('\r');
	xfunc_out(c);
}




void xputs (const char* str)
{
	while (*str)
		xputc(*str++);
}




void xitoa (long val, int radix, int len)
{
	BYTE c, r, sgn = 0, pad = ' ';
	BYTE s[20], i = 0;
	DWORD v;


	if (radix < 0) {
		radix = -radix;
		if (val < 0) {
			val = -val;
			sgn = '-';
		}
	}
	v = val;
	r = radix;
	if (len < 0) {
		len = -len;
		pad = '0';
	}
	if (len > 20) return;
	do {
		c = (BYTE)(v % r);
		if (c >= 10) c += 7;
		c += '0';
		s[i++] = c;
		v /= r;
	} while (v);
	if (sgn) s[i++] = sgn;
	while (i < len)
		s[i++] = pad;
	do
		xputc(s[--i]);
	while (i);
}




void xprintf (const char* str, ...)
{
	va_list arp;
	int d, r, w, s, l;


	va_start(arp, str);

	while ((d = *str++) != 0) {
		if (d != '%') {
			xputc(d); continue;
		}
		d = *str++; w = r = s = l = 0;
		if (d == '0') {
			d = *str++; s = 1;
		}
		while ((d >= '0')&&(d <= '9')) {
			w += w * 10 + (d - '0');
			d = *str++;
		}
		if (s) w = -w;
		if (d == 'l') {
			l = 1;
			d = *str++;
		}
		if (!d) break;
		if (d == 's') {
			xputs(va_arg(arp, char*));
			continue;
		}
		if (d == 'c') {
			xputc((char)va_arg(arp, int));
			continue;
		}
		if (d == 'u') r = 10;
		if (d == 'd') r = -10;
		if (d == 'X') r = 16;
		if (d == 'b') r = 2;
		if (!r) break;
		if (l) {
			xitoa((long)va_arg(arp, long), r, w);
		} else {
			if (r > 0)
				xitoa((unsigned long)va_arg(arp, int), r, w);
			else
				xitoa((long)va_arg(arp, int), r, w);
		}
	}

	va_end(arp);
}




void put_dump (const BYTE *buff, DWORD ofs, int cnt)
{
	BYTE n;


	xprintf("%08lX ", ofs);
	for(n = 0; n < cnt; n++)
		xprintf(" %02X", buff[n]);
	xputs("  ");
	for(n = 0; n < cnt; n++) {
		if ((buff[n] < 0x20)||(buff[n] >= 0x7F))
			xputc('.');
		else
			xputc(buff[n]);
	}
	xputc('\n');
}




void get_line (char *buff, int len)
{
	char c;
	int idx = 0;


	for (;;) {
		c = xgetc();
		if (c == '\r') break;
		if ((c == '\b') && idx) {
			idx--; xputc(c);
		}
		if (((BYTE)c >= ' ') && (idx < len - 1)) {
			buff[idx++] = c; xputc(c);
		}
	}
	buff[idx] = 0;
	xputc('\n');
}



static
FRESULT scan_files (
		char* path		/* Pointer to the path name working buffer */
)
{
	DIR dirs;
	FRESULT res;
	BYTE i;
	char *fn;


	if ((res = f_opendir(&dirs, path)) == FR_OK) {
		i = strlen(path);
		while (((res = f_readdir(&dirs, &Finfo)) == FR_OK) && Finfo.fname[0]) {
			Sleep(0);
#if _USE_LFN
			fn = *Finfo.lfname ? Finfo.lfname : Finfo.fname;
#else
			fn = Finfo.fname;
#endif
			if (Finfo.fname[0] == '.') continue;
			if (Finfo.fattrib & AM_DIR) {
				acc_dirs++;
				*(path+i) = '/'; strcpy(path+i+1, fn);
				res = scan_files(path);
				*(path+i) = '\0';
				if (res != FR_OK) break;
			} else {
				acc_files++;
				acc_size += Finfo.fsize;
			}
		}
	}

	return res;
}



static
void put_rc (FRESULT rc)
{
	const char *p;
	static const char str[] =
			"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
			"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
			"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0";
	FRESULT i;

	for (p = str, i = 0; i != rc && *p; i++) {
		while(*p++);
	}
	xprintf("rc=%u FR_%s\n", (UINT)rc, p);
}

static void invalid_arg(void)
{
	xprintf("invalid arg\n");
}

UINT freadcb_test(uint8_t* buf, UINT size, UINT* rs)
{
	uint16_t a = f_read(&File1, buf, size, rs);
//	xprintf("read cb test, %d, %d, %d\n", size, *rs, a);
	return *rs;
}
UINT freadcb_udp(uint8_t* buf, UINT size, UINT* rs)
{
	int emptyflg = 0;
	while(1) {
		uint16_t curSize = _wiz_read2(k_sockToBaseAddr[0]+WIZSxRXRSR);
		if (curSize > 3500) {
			xprintf("RX buffer will full\n");
		}
		if (curSize !=0 && curSize >= (packetsize+8)) {
			emptyflg = 0;
			break;
		}
		if (curSize == 0) {
			emptyflg++;
			xprintf("RX buffer empty, %d\n", emptyflg);
		}
	}
		*rs = WIZRecv(0, buf, size);
		WIZSend(0, (uint8_t*)"ACK", 4);

	return *rs;
}

UINT freadcb_tcp(uint8_t* buf, UINT size, UINT* rs)
{
	while(1) {
			uint16_t curSize = _wiz_read2(k_sockToBaseAddr[0]+WIZSxRXRSR);
			if (curSize >= size) {
				break;
			}
		}

		*rs = _wiz_recv_data(0, buf, size);
return 0;
	}

void monitor (void)
{
	char *ptr, *ptr2;
	long p1, p2, p3, p4;
	BYTE res, b1;
	WORD w1;
	UINT s1, s2, cnt, blen = sizeof(Buff);
	DWORD ofs = 0, sect = 0;
	FATFS *fs;				/* Pointer to file system object */

	// For OLED
	uint8_t buf[1024];
	UINT readsize;

	UINT tmpi;
	BYTE tmpc;

	xfunc_out = (void(*)(char))uart0_put;
	xputs("\nLPC2388 evaluation board test monitor\n");

	for (;;) {
		xputc('>');
		ptr = Linebuf;
		get_line(ptr, sizeof(Linebuf));

		switch (*ptr++) {

		case 'm' :
			switch (*ptr++) {
			case 'd' :	/* md <address> [<count>] - Dump memory */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2)) p2 = 128;
				for (ptr=(char*)p1; p2 >= 16; ptr += 16, p2 -= 16)
					put_dump((BYTE*)ptr, (UINT)ptr, 16);
				if (p2) put_dump((BYTE*)ptr, (UINT)ptr, p2);
				break;
			case 'b' : /* mb <address> <byte> Write byte to memory */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
				xprintf("%X - %X\n", p1, p2);
				xprintf("%X\n", *((UINT*)p1));
				tmpi = *((UINT*)p1);
				tmpc = (BYTE)p2;
				tmpi = (tmpi & 0xffffff00) | tmpc;
				*((UINT*)p1) = tmpi;
						xprintf("%X\n", *((UINT*)p1));
				break;
			case 'w' : /* mw <address> <word> Write word to memory */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
				*((WORD*)p1) = (WORD)p2;
				break;
			}
			break;

		case 'n' :
			switch (*ptr++) {
			case 'w' :	/* nw <address> <data> ... - Write net data*/
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				while (xatoi(&ptr, &p2)) {
					_wiz_write(p1, (uint8_t)p2);
					xprintf("0x%X - %d(0x%X)\n", p1, p2, p2);
					p1++;
				}
//				xprintf("\n");
				break;

			case 'r' : /* nr <address> <len> - Read net data*/
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }

				for (tmpi = 0; tmpi < p2; tmpi++) {
					tmpc = _wiz_read(p1+tmpi);
					xprintf("0x0%X - %d(0x%0X)\n", p1+tmpi, tmpc, tmpc);
				}
				break;

			case 'p' : /* np 1 or 0 - Power net */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				WIZPower(p1);
				if (p1) {
					const uint8_t ip[] = {10, 0, 1, 201};
					const uint8_t hwaddr[] = {0, 8, 0xdc, 1, 2, 3};
					WIZSetMAC(hwaddr);
					WIZSetIP(ip);

				}
				break;
			case 's' : /* ns Start media server */
				NetTaskStart();
				break;
			case 'e' : /* ne Disconnect Socket 1 */
				WIZDisconnect(1);
				break;

			case 'c' : /* nc - Connect to device */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				uint8_t buf1[1024];
				if (p1 == 2) {
					/* connect to OLED */
					OLEDWriteCommand(OLEDCMDMemoryWrite);
					OLEDWriteDataStart();
					while(1) {
						readsize = _wiz_recv_data(0, buf1, 1024);
						if (readsize == 0) {
							xprintf("recv error\n");
						}
						int i;
						for (i = 0; i < readsize; i++) {
							OLEDWrite(buf1[i]);
						}
					}
					OLEDWriteEnd();
				}
				break;

			case 't' : /* nt <test> - Net test */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (p1 == 1) { /* Prepare FIFO  */
					_wiz_prepare_fifo_test();
				}
				if (p1 == 2) {
					if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
					/* Open UDP port at 5000 dst 10.0.1.X:5000 */
					WIZSocketCreate(0, WIZProtocolUDP, 5000, 0);
					uint8_t dip[4];
					dip[0] = 10;
					dip[1] = 0;
					dip[2] = 1;
					dip[3] = p2;
					WIZSendTo(0, NULL, 0, dip, 5001);
				}
				if (p1 == 3) { /* send message test */
					uint8_t* helo = (uint8_t*)"hello world\n";
					if (_wiz_send_data(0, helo, 13) == 0 ) {
						xprintf("buffer full\n");
					}
				}
				if (p1 == 4) { /* send byte test */
					if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
					_wiz_test_send(p2);
				}
				if (p1 == 5) { /* Show recv UDP data */
					uint8_t b[1024];
					uint8_t sip[4];
					uint16_t sport;
					tmpi = _wiz_recv_data_udp(0, b, 1024, sip, &sport);
					xprintf("recv %d byte. from %d.%d.%d.%d:%d\n", tmpi, sip[0], sip[1], sip[2], sip[3], sport);
					int i;
					for (i = 0; i < tmpi; i+= 16) {
						put_dump(b+i, i, 16);
					}
				}
				if (p1 == 6) { /* Show recv data */
					uint8_t b[1024];
					tmpi = _wiz_recv_data(0, b, 1024);
					xprintf("recv %d byte\n", tmpi);
					int i;
					for (i = 0; i < tmpi; i+= 16) {
						put_dump(b+i, i, 16);
					}
				}
				if (p1 == 7) {
					if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
					uint8_t dstip[4];
					dstip[0] = 10;
					dstip[1] = 0;
					dstip[2] = 1;
					dstip[3] = p2;
					/* connect to TCP port 5000 */
					xprintf("open=%d\n", WIZSocketCreate(0, WIZProtocolTCP, 5000, 0));
					xprintf("connect=%d\n", WIZConnect(0, dstip, 5001));
				}
				if (p1 == 8) { /* play from udp */
					if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
					packetsize = p2;
					VSPlay(freadcb_udp);
				}
				break;
			}
			break;


		case 'p' :
			switch (*ptr++) {
			case 'p' :	/* pp <name> - Play file */
				while (*ptr == ' ') ptr++;
				xprintf("not implemented\n");
				//xprintf("play=%d\n", playfile(ptr));
				break;
			case 'a' :	/* pa <name> - Play file (rawmode) */
				while (*ptr == ' ') ptr++;
				xprintf("play=%d\n", VSPlayRaw(ptr));
				break;
			case 'b' :	/* pb - Play file (callback mode) */
				while (*ptr == ' ') ptr++;
				VSPlay(freadcb_test);
				break;
			case 's' :	/* ps - Stop music */
				VSPlayStop();
				break;
			case 'd' : /* pd - Reset device */
				VSReset();
				break;

			case 'v' : /* pv <src> <val> - Set current volume (1=AD, 2=Ext)*/
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
				VSSetVolumeControl(p1);
				VSSetVolume(p2);
				break;
			case 'r' : /* pr <reg> - VS register read */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				xprintf("0x%X = 0x%X\n", p1, _vs_read(p1));
				break;
			case 'w' : /* pw <reg> <data> - VS regsiter write */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
				_vs_write(p1, p2);
			}
			break;

		case 'u' :
			switch (*ptr++) {
			case 's' :	/* us - Show Power status */
				tmpc = FIO4PIN3;
				xprintf("FIO4PIN3=0x%X\n", tmpc);
				xprintf("BSTAT=%d\n", (tmpc >> 6) & 0x03);
				xprintf("BUSBPG=%d\n", (tmpc >> 1) & 0x01);
				break;
			case 'h' : /* uh - Set USB high current */
				FIO4SET3 = 0x01;
				break;
			case 'l' : /* ul - Set USB low current */
				FIO4CLR3 = 0x01;
				break;
			}
			break;



		case 'o' :
			switch (*ptr++) {
			case 'i' : /* oi - Send Pixel data from current file */
				OLEDWriteCommand(OLEDCMDMemoryWrite);
				OLEDWriteDataStart();
				while ((tmpi = f_read(&File1, buf, 1024, &readsize)) == FR_OK) {
					if (readsize <= 0) break;
					int i;
					for (i = 0; i < readsize; i++) {
						OLEDWrite(buf[i]);
					}
				}
				OLEDWriteEnd();
				put_rc(tmpi);
				break;

			case 'v' : /* ov <1> or <0> - OLED VCC control */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (p1 == 1) {
					OLEDVccON();
				} else {
					OLEDVccOFF();
				}
				break;

			case 'p' : /* op <data> <x> <y> - Put char at x, y */
				if (!xatoi(&ptr, &p1))  { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2))  { invalid_arg(); break; }
				if (!xatoi(&ptr, &p3))  { invalid_arg(); break; }

				OLEDPutFont((char)p1, p2, p3);

				break;

			case 's' : /* os <string> - Print String */
				while (*ptr == ' ') ptr++;
				OLEDPrintString(ptr);
				break;

			case 'c' : /* oc <data> - Send OLED command */
				if (!xatoi(&ptr, &p1))  { invalid_arg(); break; }
				OLEDWriteCommand((uint8_t)p1);
				break;
			case 'd' : /* od <d1> <d2> <d3> ...  - Send OLED Data */
				OLEDWriteDataStart();
				int byt = 0;
				while (xatoi(&ptr, &p1)) {
					OLEDWrite((uint8_t)p1);
					xprintf("%X ", p1);
					byt++;
				}
				xprintf("\ntotal %d byte sent.\n", byt);
				OLEDWriteEnd();
				break;
			case 'f' : /* od <a> <b> <c> <count>  - Fill OLED Pixel Data */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p3)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p4)) { invalid_arg(); break; }
				OLEDWriteCommand(OLEDCMDMemoryWrite);
				OLEDWriteDataStart();
				int i;
				for (i = 0; i < p4; i++) {
					OLEDWrite((uint8_t)p1);
					OLEDWrite((uint8_t)p2);
					OLEDWrite((uint8_t)p3);
				}
				OLEDWriteEnd();
				break;
			case 'r' : /* or <x> <y> (to) <x> <y>  - Set OLED draw rect */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p2)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p3)) { invalid_arg(); break; }
				if (!xatoi(&ptr, &p4)) { invalid_arg(); break; }
				OLEDWriteCommand(OLEDCMDSetColumnAddress);
				OLEDWriteDataStart();
				OLEDWrite((uint8_t)p1);
				OLEDWrite((uint8_t)p3);
				OLEDWriteEnd();

				OLEDWriteCommand(OLEDCMDSetRowAddress);
				OLEDWriteDataStart();
				OLEDWrite((uint8_t)p2);
				OLEDWrite((uint8_t)p4);
				OLEDWriteEnd();

				xprintf("set rect %d, %d - %d, %d\n",p1,p2,p3,p4);
				break;
			}

		case 'd' :
			switch (*ptr++) {
			case 'd' :	/* dd [<sector>] - Dump secrtor */
				if (!xatoi(&ptr, &p2)) p2 = sect;
				res = disk_read(0, Buff, p2, 1);
				if (res) { xprintf("rc=%d\n", (WORD)res); break; }
				sect = p2 + 1;
				xprintf("Sector:%lu\n", p2);
				for (ptr=(char*)Buff, ofs = 0; ofs < 0x200; ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16);
				break;

			case 'i' :	/* di - Initialize disk */
				xprintf("rc=%d\n", (WORD)disk_initialize(0));
				break;

			case 's' :	/* ds - Show disk status */
				if (disk_ioctl(0, GET_SECTOR_COUNT, &p2) == RES_OK)
				{ xprintf("Drive size: %lu sectors\n", p2); }
				if (disk_ioctl(0, GET_SECTOR_SIZE, &w1) == RES_OK)
				{ xprintf("Sector size: %u\n", w1); }
				if (disk_ioctl(0, GET_BLOCK_SIZE, &p2) == RES_OK)
				{ xprintf("Erase block size: %lu sectors\n", p2); }
				if (disk_ioctl(0, MMC_GET_TYPE, &b1) == RES_OK)
				{ xprintf("Card type: %u\n", b1); }
				if (disk_ioctl(0, MMC_GET_CSD, Buff) == RES_OK)
				{ xputs("CSD:\n"); put_dump(Buff, 0, 16); }
				if (disk_ioctl(0, MMC_GET_CID, Buff) == RES_OK)
				{ xputs("CID:\n"); put_dump(Buff, 0, 16); }
				if (disk_ioctl(0, MMC_GET_OCR, Buff) == RES_OK)
				{ xputs("OCR:\n"); put_dump(Buff, 0, 4); }
				if (disk_ioctl(0, MMC_GET_SDSTAT, Buff) == RES_OK) {
					xputs("SD Status:\n");
					for (s1 = 0; s1 < 64; s1 += 16) put_dump(Buff+s1, s1, 16);
				}
				break;
			}
			break;

		case 'b' :
			switch (*ptr++) {
			case 'd' :	/* bd <addr> - Dump R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				for (ptr=(char*)&Buff[p1], ofs = p1, cnt = 32; cnt; cnt--, ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16);
				break;

			case 'e' :	/* be <addr> [<data>] ... - Edit R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				if (xatoi(&ptr, &p2)) {
					do {
						Buff[p1++] = (BYTE)p2;
					} while (xatoi(&ptr, &p2));
					break;
				}
				for (;;) {
					xprintf("%04X %02X-", (WORD)(p1), (WORD)Buff[p1]);
					get_line(Linebuf, sizeof(Linebuf));
					ptr = Linebuf;
					if (*ptr == '.') break;
					if (*ptr < ' ') { p1++; continue; }
					if (xatoi(&ptr, &p2))
						Buff[p1++] = (BYTE)p2;
					else
						xputs("???\n");
				}
				break;

			case 'r' :	/* br <sector> [<n>] - Read disk into R/W buffer */
				if (!xatoi(&ptr, &p2)) break;
				if (!xatoi(&ptr, &p3)) p3 = 1;
				xprintf("rc=%u\n", (WORD)disk_read(0, Buff, p2, p3));
				break;

			case 'w' :	/* bw <sector> [<n>] - Write R/W buffer into disk */
				if (!xatoi(&ptr, &p2)) break;
				if (!xatoi(&ptr, &p3)) p3 = 1;
				xprintf("rc=%u\n", (WORD)disk_write(0, Buff, p2, p3));
				break;

			case 'f' :	/* bf <n> - Fill working buffer */
				if (!xatoi(&ptr, &p1)) break;
				memset(Buff, (BYTE)p1, sizeof(Buff));
				break;

			}
			break;

		case 'f' :
			switch (*ptr++) {

			case 'i' :	/* fi - Force initialized the logical drive */
				put_rc(f_mount(0, &Fatfs[0]));
				break;

			case 's' :	/* fs [<path>] - Show logical drive status */
				while (*ptr == ' ') ptr++;
				res = f_getfree(ptr, (DWORD*)&p2, &fs);
				if (res) { put_rc(res); break; }
				xprintf("FAT type = %u\nBytes/Cluster = %lu\nNumber of FATs = %u\n"
						"Root DIR entries = %u\nSectors/FAT = %lu\nNumber of clusters = %lu\n"
						"FAT start (lba) = %lu\nDIR start (lba,clustor) = %lu\nData start (lba) = %lu\n",
						(WORD)fs->fs_type, (DWORD)fs->csize * 512, (WORD)fs->n_fats,
						fs->n_rootdir, fs->sects_fat, (DWORD)fs->max_clust - 2,
						fs->fatbase, fs->dirbase, fs->database
				);
				acc_size = acc_files = acc_dirs = 0;
#if _USE_LFN
				Finfo.lfname = Lfname;
				Finfo.lfsize = sizeof(Lfname);
#endif
				res = scan_files(ptr);
				if (res) { put_rc(res); break; }
				xprintf("%u files, %lu bytes.\n%u folders.\n"
						"%lu KB total disk space.\n%lu KB available.\n",
						acc_files, acc_size, acc_dirs,
						(fs->max_clust - 2) * (fs->csize / 2), p2 * (fs->csize / 2)
				);
				break;

			case 'l' :	/* fl [<path>] - Directory listing */
				while (*ptr == ' ') ptr++;
				res = f_opendir(&Dir, ptr);
				if (res) { put_rc(res); break; }
				p1 = s1 = s2 = 0;
#if _USE_LFN
				Finfo.lfname = Lfname;
				Finfo.lfsize = sizeof(Lfname);
#endif
				for(;;) {
					res = f_readdir(&Dir, &Finfo);
					if ((res != FR_OK) || !Finfo.fname[0]) break;
					if (Finfo.fattrib & AM_DIR) {
						s2++;
					} else {
						s1++; p1 += Finfo.fsize;
					}
					xprintf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s",
							(Finfo.fattrib & AM_DIR) ? 'D' : '-',
									(Finfo.fattrib & AM_RDO) ? 'R' : '-',
											(Finfo.fattrib & AM_HID) ? 'H' : '-',
													(Finfo.fattrib & AM_SYS) ? 'S' : '-',
															(Finfo.fattrib & AM_ARC) ? 'A' : '-',
																	(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
																	(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63, Finfo.fsize, Finfo.fname);
#if _USE_LFN
					xprintf("  %s\n", Lfname);
#else
					xputc('\n');
#endif
				}
				xprintf("%4u File(s),%10lu bytes total\n%4u Dir(s)", s1, p1, s2);
				if (f_getfree(ptr, (DWORD*)&p1, &fs) == FR_OK)
					xprintf(", %10lu bytes free\n", p1 * fs->csize * 512);
				break;

			case 'o' :	/* fo <mode> <file> - Open a file */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				while (*ptr == ' ') ptr++;
				put_rc(f_open(&File1, ptr, (BYTE)p1));
				break;

			case 'c' :	/* fc - Close a file */
				put_rc(f_close(&File1));
				break;

			case 'e' :	/* fe - Seek file pointer */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				res = f_lseek(&File1, p1);
				put_rc(res);
				if (res == FR_OK)
					xprintf("fptr=%lu(0x%lX)\n", File1.fptr, File1.fptr);
				break;

			case 'd' :	/* fd <len> - read and dump file from current fp */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				ofs = File1.fptr;
				while (p1) {
					if ((UINT)p1 >= 16) { cnt = 16; p1 -= 16; }
					else 				{ cnt = p1; p1 = 0; }
					res = f_read(&File1, Buff, cnt, &cnt);
					if (res != FR_OK) { put_rc(res); break; }
					if (!cnt) break;
					put_dump(Buff, ofs, cnt);
					ofs += 16;
				}
				break;

			case 'r' :	/* fr <len> - read file */
				if (!xatoi(&ptr, &p1)) { invalid_arg(); break; }
				p2 = 0;
				Timer = 0;
				while (p1) {
					if ((UINT)p1 >= blen) {
						cnt = blen; p1 -= blen;
					} else {
						cnt = p1; p1 = 0;
					}
					res = f_read(&File1, Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				xprintf("%lu bytes read with %lu kB/sec.\n", p2, p2 / Timer);
				break;

			case 'w' :	/* fw <len> <val> - write file */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				memset(Buff, (BYTE)p2, blen);
				p2 = 0;
				Timer = 0;
				while (p1) {
					if ((UINT)p1 >= blen) {
						cnt = blen; p1 -= blen;
					} else {
						cnt = p1; p1 = 0;
					}
					res = f_write(&File1, Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				xprintf("%lu bytes written with %lu kB/sec.\n", p2, p2 / Timer);
				break;

			case 'n' :	/* fn <old_name> <new_name> - Change file/dir name */
				while (*ptr == ' ') ptr++;
				ptr2 = strchr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				put_rc(f_rename(ptr, ptr2));
				break;

			case 'u' :	/* fu <name> - Unlink a file or dir */
				while (*ptr == ' ') ptr++;
				put_rc(f_unlink(ptr));
				break;

			case 'v' :	/* fv - Truncate file */
				put_rc(f_truncate(&File1));
				break;

			case 'k' :	/* fk <name> - Create a directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_mkdir(ptr));
				break;

			case 'a' :	/* fa <atrr> <mask> <name> - Change file/dir attribute */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				while (*ptr == ' ') ptr++;
				put_rc(f_chmod(ptr, p1, p2));
				break;

			case 't' :	/* ft <year> <month> <day> <hour> <min> <sec> <name> - Change timestamp */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				Finfo.fdate = ((p1 - 1980) << 9) | ((p2 & 15) << 5) | (p3 & 31);
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				Finfo.ftime = ((p1 & 31) << 11) | ((p2 & 63) << 5) | ((p3 >> 1) & 31);
				while (*ptr == ' ') ptr++;
				put_rc(f_utime(ptr, &Finfo));
				break;
#if _FS_RPATH
			case 'g' :	/* fg <path> - Change current directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_chdir(ptr));
				break;

			case 'j' :	/* fj <drive#> - Change current drive */
				if (xatoi(&ptr, &p1)) {
					put_rc(f_chdrive((BYTE)p1));
				}
				break;
#endif
#if _USE_MKFS
			case 'm' :	/* fm <partition rule> <cluster size> - Create file system */
				if (!xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				xprintf("The drive %u will be formatted. Are you sure? (Y/n)=", p1);
				get_line(ptr, sizeof(Linebuf));
				if (*ptr == 'Y')
					put_rc(f_mkfs(0, (BYTE)p2, (WORD)p3));
				break;
#endif
			case 'z' :	/* fz [<rw size>] */
				if (xatoi(&ptr, &p1) && p1 >= 1 && p1 <= sizeof(Buff))
					blen = p1;
				xprintf("blen=%u\n", blen);
				break;
			}
			break;
		}
	}
}


