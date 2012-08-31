/*
 *  Oled.c
 *  Unkoled
 *
 *  Created by novi on 21/03/13.
 *  Copyright 2009 novi. All rights reserved.
 *
 */
#include "Oled.h"
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include "mpfont.h"

//#define mpfontd(idx) (mpfont[(idx)])

static uint8_t m_OLEDFontLineCount = 0;
static uint8_t m_OLEDFontScrollCount = 0;
static uint8_t m_OLEDFontLinePtr = 0;




extern void OLEDSetVerticalScroll(uint8_t top, uint8_t size, uint8_t bottom)
{
	//	OLEDWriteCommand(OLEDCMDVerticalScrollingDefinition);
	OLEDWriteDataStart();
	OLEDWrite(OLEDTopOffset+top);
	OLEDWrite(size);
	OLEDWrite(bottom);
	OLEDWriteEnd();
}

extern void OLEDScrollVertical(uint8_t ptr)
{
	//	OLEDWriteCommand(OLEDCMDVerticalScrollingStartAddress);
	OLEDWriteDataStart();
	OLEDWrite(OLEDTopOffset+ptr);
	OLEDWriteEnd();
}


void OLEDPrintString(const char* str)
{
	int i;
	// Calc line require
	for (i = 0; i < ceil((float)strlen(str)/(float)OLEDFontMaxX); i++) {
		OLEDPrintStringLine(str+(OLEDFontMaxX*i));
	}
}

void OLEDPutFont(char cc, uint8_t x, uint8_t y)
{
	static OLEDColor cl = {0x00, 0xff,0};
	static OLEDColor bg = {0x0,0x00,0};
	OLEDPutFontWithColor(cc, x, y, cl, bg);
}

void OLEDPrintStringLine(const char* str)
{
	uint8_t ypos;
	if (m_OLEDFontLineCount < OLEDFontMaxY) {
		m_OLEDFontLineCount++;
	} else {
		// Need scroll
		if (m_OLEDFontScrollCount >= OLEDFontMaxY) {
			m_OLEDFontScrollCount = 0;
		}
		m_OLEDFontScrollCount++;
		OLEDSetVerticalScroll(0, OLEDFontMaxY*OLEDFontHeight, 0);
		OLEDScrollVertical(OLEDFontHeight*m_OLEDFontScrollCount);
	}
	
	ypos = m_OLEDFontLinePtr*OLEDFontHeight;
	
	
	m_OLEDFontLinePtr++;
	if (m_OLEDFontLinePtr >= OLEDFontMaxY) {
		m_OLEDFontLinePtr = 0;
	}
	
	char chara;
	uint8_t xpos = 0;
	uint8_t i = 0;
	while ((chara = *(str+i))) {
		if (i+1 > OLEDFontMaxX) {
			break;
		}
		OLEDPutFont(chara, xpos, ypos);
		xpos += OLEDFontWidth;
		i++;
	}
	
	if (!(m_OLEDFontLineCount < OLEDFontMaxY)) {
		// need scroll
		// fill empty space
		uint8_t j;
		for (j = 0; j < OLEDFontMaxX-i; j++) {
			OLEDPutFont(0x20, xpos, ypos);
			xpos += OLEDFontWidth;
		}
	}
}

void OLEDPutFontWithColor(char cc, uint8_t x, uint8_t y, OLEDColor color, OLEDColor bg)
{
	int indx = 0;
	if (cc <= 0x20 || cc > 0x7e) {
		indx = 0;
	} else {
		indx = cc-0x20;
	}

	unsigned int fontdata = mpfont[indx];

	OLEDSetRectX(x, x+4);
	OLEDSetRectY(y, y+5);

	int i;
	OLEDWriteCommand(OLEDCMDMemoryWrite);
	OLEDWriteDataStart();
	for (i = 0; i < 30; i++) {
		if (0x80000000 & fontdata) {
			OLEDWritePixel(color);
		} else {
			OLEDWritePixel(bg);
		}

		fontdata <<= 1;
	}
	OLEDWriteEnd();
}


void OLEDWriteCommand(uint8_t cmd)
{
	OLEDWriteCommandStart();
		OLEDWrite(cmd);
		OLEDWriteEnd();
}

void OLEDWriteOption(uint8_t cmd, uint8_t val)
{
	OLEDWriteCommandStart();
	OLEDWrite(cmd);
	OLEDWriteDataStart();
	OLEDWrite(val);
	OLEDWriteEnd();
}

void OLEDWriteOptions2(uint8_t cmd, uint8_t val1, uint8_t val2)
{
	OLEDWriteCommandStart();
	OLEDWrite(cmd);
	OLEDWriteDataStart();
	OLEDWrite(val1);
	OLEDWrite(val2);
	OLEDWriteEnd();
	OLEDMODE_COMMAND();
}

void OLEDWriteOptions3(uint8_t cmd, uint8_t val1, uint8_t val2, uint8_t val3)
{
	OLEDWriteCommandStart();
	OLEDWrite(cmd);
	OLEDWriteDataStart();
	OLEDWrite(val1);
	OLEDWrite(val2);
	OLEDWrite(val3);
	OLEDWriteEnd();
	OLEDMODE_COMMAND();
}
void OLEDSetRectY(uint8_t yy0, uint8_t yy1)
{
	OLEDWriteCommand(OLEDCMDSetRowAddress);
	OLEDWriteDataStart();
	OLEDWrite(OLEDTopOffset+yy0);
	OLEDWrite(OLEDTopOffset+yy1);
	OLEDWriteEnd();
}

void OLEDSetRectX(uint8_t x0, uint8_t x1)
{
	OLEDWriteCommand(OLEDCMDSetColumnAddress);
	OLEDWriteDataStart();
	OLEDWrite(x0);
	OLEDWrite(x1);
	OLEDWriteEnd();
}


void OLEDWritePixel(OLEDColor color)
{
#if !OLEDCOLOR_262K
		// 8bit, 65k
		OLEDDATA = color.a;
		OLEDDATA_WRITE();
		
		OLEDDATA = color.b;
		OLEDDATA_WRITE();
#endif

#if OLEDCOLOR_262K
		OLEDDATA = color.a;
		OLEDDATA_WRITE();
		
		OLEDDATA = color.b;
		OLEDDATA_WRITE();
		
		OLEDDATA = color.c;
		OLEDDATA_WRITE();
#endif

}


void OLEDDrawLine(uint8_t px1, uint8_t py1, uint8_t px2, uint8_t py2, OLEDColor color)
{
	OLEDWriteCommand(0x21);
	OLEDWriteDataStart();
	OLEDWrite(px1);
	OLEDWrite(py1);
	OLEDWrite(px2);
	OLEDWrite(py2);
	OLEDWrite(color.a);
	OLEDWrite(color.b);
	OLEDWrite(color.c);
	OLEDWriteEnd();
}
