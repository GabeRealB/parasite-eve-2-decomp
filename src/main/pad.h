#ifndef MAIN_PRIVATE_PAD_H
#define MAIN_PRIVATE_PAD_H

#include "types.h"

#include "pad_types.h"

extern PadRawPort Pad_RawPorts[2];

void Pad_Init(void);

s32 Pad_CheckSpecialCombo(void);

void Pad_UpdatePort0(void);

#endif // MAIN_PRIVATE_PAD_H
