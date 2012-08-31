

#include "integer.h"
#include "ff.h"
#include "comm.h"

int xatoi (char**, long*);
char xgetc_n (void);
char xgetc (void);
void xputc (char);
void xputs (const char*);
void xitoa (long, int, int);
void xprintf (const char*, ...);
void put_dump (const BYTE*, DWORD ofs, int cnt);
void get_line (char*, int len);
void filer(char*, FIL*, DIR*, FILINFO*);
void io_scan(void);

#define BTN_UP		0x05
#define BTN_DOWN	0x18
#define BTN_LEFT	0x13
#define BTN_RIGHT	0x04
#define BTN_OK		0x0D
#define BTN_CAN		0x1B
#define BTN_POWER	0x7F
#define CMD_LOWBAT	0x1F
