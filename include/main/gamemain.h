#ifndef GAMEMAIN_H
#define GAMEMAIN_H

#include "common.h"

#include <psyq/libgte.h>

// =============================================================================
// Functions — src/main/gamemain.c
// =============================================================================

/// Game entry point. Called by `main`.
void GameMain(void);

void F179D4_ClearOTag(s16 tableIdx);
void GameMain_Init(void);
void GameMain_Loop(void);
u32  GameMain_GetResetCount(void);
void Display_FlipDraw(s32 arg0);
void Display_VSyncCallback(void);
void GameMain_ShowLoading(s32 arg0);
/// "PAUSE!" overlay text for GameMain_ShowLoading (@ VA 0x80013404).
extern const u8 GameMain_PauseText[];
void            Gfx_InitCoordinateTrees(void);
void            GameMain_SetFrameTiming(s32 arg0);
void            Gfx_InitGraph(void);
/// Normalize light direction for GTE (src/main/gfxmtx.c).
void Gfx_NormalizeLightDir(VECTOR* light, SVECTOR* out);

#endif // GAMEMAIN_H
