#ifndef GAMEFLAG_H
#define GAMEFLAG_H

#include "common.h"

// Packed 4-bit game flags (src/main/gameflag.c)

void GameFlag_SetNibble(s32 index, s32 value);
s32  GameFlag_GetNibble(s32 index);

#endif // GAMEFLAG_H
