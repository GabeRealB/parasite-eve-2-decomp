#ifndef GAMEFLAG_H
#define GAMEFLAG_H

#include "common.h"

// Packed 4-bit game flags (src/main/gameflag.c)

void GameFlag_SetNibble(s32 index, s32 value);
s32  GameFlag_GetNibble(s32 index);
/// Returns Pad_CheckButtons(0, 1, 0x800); non-void so callers can branch on v0.
s32 Pad_CheckFlag800(void);

#endif // GAMEFLAG_H
