/*
 * play.h
 *
 *  Created on: 2009/10/08
 *      Author: novi
 */

#ifndef PLAY_H_
#define PLAY_H_
#include "integer.h"
#include <inttypes.h>

#define VSVolumeControlAD 0x1
#define VSVolumeControlExt 0x2

UINT VSPlayCallback(uint8_t* buf, UINT requiresize, UINT* gotsize);


void VSPlayerTask(void* args);
void VSPlay(UINT (*VSPlayCallback)(uint8_t* buf, UINT requiresize, UINT* gotsize));
void VSReset(void);
void VSPlayStop(void);
void VSSetVolume(uint8_t v);
void VSSetVolumeControl(uint8_t vc);

uint8_t VSPlayRaw(const char* fname);

void _vs_write(uint8_t addr, uint16_t data);
uint16_t _vs_read(uint8_t addr);




#endif /* PLAY_H_ */
