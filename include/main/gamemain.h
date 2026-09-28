#ifndef MAIN_GAMEMAIN_H
#define MAIN_GAMEMAIN_H

#include "types.h"

extern volatile s32 GameMain_HaltFlags;

/// State of the gameplay random generator, kept in the resident image so every
/// overlay draws from one sequence. A draw steps it to `state * 5 +
/// 0x71357911` and takes the high halfword. Starting a stream saves it and
/// resets it to 0, and `Gp_RestoreStreamRng` puts it back, so play resumes the
/// sequence it left.
extern u32 Gp_LcgState;

u32 GameMain_GetResetCount(void);

void GameMain_SetFrameTiming(s32 arg0);

#endif // MAIN_GAMEMAIN_H
