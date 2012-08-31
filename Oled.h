/*
 *  Oled.h
 *  Unkoled
 *
 *  Created by novi on 21/03/13.
 *  Copyright 2009 novi. All rights reserved.
 *
 */
#ifndef __OLED_H_
#define __OLED_H_

#include "LPC2300.h"
#include <inttypes.h>


// OLED Color depth
// 0: 65k
// 1: 262k
// NOTE: You must set Color register(CMDReMapColorDepth)
#define OLEDCOLOR_262K 1

// Definition of OLED module
#define OLEDTopOffset 0
#define OLEDWidth 128
#define OLEDHeight 128
#define OLEDXMax 127
#define OLEDYMax 127
#define OLEDFontWidth 6
#define OLEDFontHeight 6
#define OLEDFontMaxX 21 // OLEDWidth/FontWidth
#define OLEDFontMaxY 21


typedef struct {
	uint8_t a;
	uint8_t b;
	uint8_t c;
} OLEDColor;


// You can use OLEDCMD...
void OLEDWriteCommand(unsigned char cmd);
void OLEDWriteOption(uint8_t cmd, uint8_t val);
void OLEDWriteOptions2(uint8_t cmd, uint8_t val1, uint8_t val2);
void OLEDWriteOptions3(uint8_t cmd, uint8_t val1, uint8_t val2, uint8_t val3);


// Write one pixel and foward row or column
// 65k = abc[0] rg,
// 65k = abc[1] gb,
//
// 262k = abc[0] r, 
// 262k = abc[1] g,
// 262k = abc[2] b,
void OLEDWritePixel(OLEDColor color);

void OLEDSetRectY(uint8_t y0, uint8_t y1);
void OLEDSetRectX(uint8_t x0, uint8_t x1);

void OLEDPrintStringLine(const char* str);
void OLEDPrintString(const char* str);
void OLEDPutFont(char cc, uint8_t x, uint8_t y);
void OLEDPutFontWithColor(char cc, uint8_t x, uint8_t y, OLEDColor color, OLEDColor bg);

void OLEDSetVerticalScroll(uint8_t top, uint8_t size, uint8_t bottom);
void OLEDScrollVertical(uint8_t ptr);

void OLEDDrawLine(uint8_t px1, uint8_t py1, uint8_t px2, uint8_t py2, OLEDColor color);


#if OLEDCOLOR_262K
#define OLEDColorSize 3
#elif
#define OLEDColorSize 2
#endif


// Definition of Macro
// Platform dependent
#define OLEDVccON() {FIO4CLR1 = 0x8;}
#define OLEDVccOFF() {FIO4SET1 = 0x8;}
#define OLEDDATA FIO4PIN0
#define OLEDDATA_WRITE() {FIO4CLR1 = 0x1;FIO4SET1 = 0x1;}
#define OLEDMODE_DATA() {FIO4SET1 = 0x4;}
#define OLEDMODE_COMMAND() {FIO4CLR1 = 0x4;}
#define OLEDCS_ENABLE() {FIO4CLR1 = 0x2;}
#define OLEDCS_DISABLE() {FIO4SET1 = 0x2;}
#define OLEDRESET_ENABLE() {FIO4CLR1 = 0x10;}
#define OLEDRESET_DISABLE() {FIO4SET1 = 0x10;}

#define OLEDWrite(data) {OLEDDATA = (data);OLEDDATA_WRITE();}
#define OLEDWriteDataStart() {OLEDMODE_DATA();OLEDCS_ENABLE();}
#define OLEDWriteCommandStart() {OLEDMODE_COMMAND();OLEDCS_ENABLE();}
#define OLEDWriteEnd() {OLEDCS_DISABLE();}



// Definition of Commands
#define OLEDCMDNOP		 0xe3
#define OLEDCMDSoftwareReset	0xe2
//#define OLEDCMDSleepIn
//#define OLEDCMDSleepOut
//#define OLEDCMDEnablePartialDisplay
#define OLEDCMDNormalDisplayModeON 0xa4
//#define OLEDCMDDisplayInversionOFF
#define OLEDCMDDisplayInversionON 0xa7
#define OLEDCMDAllPixelsON		0xa5
#define OLEDCMDAllPixelsOFF		0xa6
//#define OLEDCMDDisableAllPixelsONOFF
#define OLEDCMDSetColumnAddress 0x15
#define OLEDCMDSetRowAddress	0x75
#define OLEDCMDMemoryWrite	0x5c
#define OLEDCMDMemoryRead	0x5d
//#define OLEDCMDPartialArea
//#define OLEDCMDVerticalScrollingDefinition
//#define OLEDCMDDisableTearingEffect
//#define OLEDCMDEnableTearingEffect
//#define OLEDCMDMemoryAccessControl
//#define OLEDCMDVerticalScrollingStartAddress
//#define OLEDCMDInterfacePixelFormat
//#define OLEDCMDWriteLuminance
//#define OLEDCMDReadLuminance
//#define OLEDCMDReadDisplayIdentificationInformation
#define OLEDCMDOTPWrite			0xc0
//#define OLEDCMDOTPMCURead
//#define OLEDCMDFunctionSelection
//#define OLEDCMDLinearGammaLookUpTable
#define OLEDCMDSetContrastForColorA 0x81
#define OLEDCMDSetContrastForColorB 0x82
#define OLEDCMDSetContrastForColorC 0x83
//#define OLEDCMDSetFirstPreChargeVoltage
//#define OLEDCMDGammaLookUpTable
//#define OLEDCMDSetDisplayOffset 0xa2
//#define OLEDCMDHorizontalScrolling
#define OLEDCMDSetMUXRatio		0xa8
//#define OLEDCMDSetPhaseLength
#define OLEDCMDSetSecondPreChargePeriod 0xb4
#define OLEDCMDSetSecondPreChargeSpeed 0x8a
#define OLEDCMDSetDisplayClockDividerOsc 0xb3
#define OLEDCMDSetVCOMH			0xbe
#define OLEDCMDGPIO				0xb5
#define OLEDCMDCommandLock	0xfd

#define OLEDCMDMasterCurrentControl 0x87
#define OLEDCMDReMapColorDepth 0xa0
#define OLEDCMDSetDisplayOffset 0xa2
#define OLEDCMDSetDisplayStartLine 0xa1
#define OLEDCMDDimModeSet 0xab
#define OLEDCMDDisplayON 0xaf
#define OLEDCMDDisplayONDim 0xac
#define OLEDCMDDisplayOFF 0xae
#define OLEDCMDPhase1Adjustment 0xb1
#define OLEDCMDSetGrayScaleTable 0xb8
#define OLEDCMDEnableGrayScaleTable 0xb9
#define OLEDCMDSetPreChargeLevel 0xbb
#define OLEDCMDDrawLine 0x21
#define OLEDCMDDrawRect 0x22
#define OLEDCMDCopy 0x23
#define OLEDCMDDimWindow 0x24
#define OLEDCMDClearWindow 0x25
#define OLEDCMDFill 0x26
#define OLEDCMDHVScrollSetting 0x27
#define OLEDCMDDeactivateHScroll 0x2e
#define OLEDCMDActivateHScroll 0x2f
#define OLEDCMDSetVScrollArea 0xa3



#endif // __OLED_H_

