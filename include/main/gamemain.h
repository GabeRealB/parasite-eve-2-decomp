#ifndef GAMEMAIN_H
#define GAMEMAIN_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

// =============================================================================
// Functions — src/main/gamemain.c
// =============================================================================

/// Game entry point. Called by `main`.
void GameMain(void);

u32  GameMain_GetResetCount(void);
void GameMain_SetFrameTiming(s32 arg0);

// Display/CD timing flags shared with the VSync path (GameMain_ShowLoading / GameMain_Loop).
extern s32          D_8005EC68;
extern s32          D_8005EC6C;
extern volatile s32 Display_PendingFlip;
/// Written by Display_VSyncCallback; read by Display_FrameFlipDraw.
extern volatile s32 D_8005EC74;
/// Cleared/set by the draw path; read by the VSync callback for lag accounting.
extern volatile s32 D_8005EC78;
extern volatile s32 GameMain_HaltFlags;

#ifndef GAMEMAIN_C
/// State of the gameplay random generator, kept in the resident image so every
/// overlay draws from one sequence. A draw steps it to `state * 5 +
/// 0x71357911` and takes the high halfword. Starting a stream saves it and
/// resets it to 0, and `Gp_RestoreStreamRng` puts it back, so play resumes the
/// sequence it left.
extern u32 Gp_LcgState;
#endif

#endif // GAMEMAIN_H
