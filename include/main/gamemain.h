#ifndef GAMEMAIN_H
#define GAMEMAIN_H

#include "common.h"

// =============================================================================
// Functions — src/main/gamemain.c
// =============================================================================

/// Game entry point. Called by `main`.
void GameMain(void);

void F179D4_ClearOTag(s16 tableIdx);
void GameMain_Init(void);
void Display_FlipDraw(s32 arg0);
void Display_VSyncCallback(void);
void GameMain_ShowLoading(s32 arg0);
void Gfx_InitCoordinateTrees(void);
void GameMain_SetFrameTiming(s32 arg0);
void Gfx_InitGraph(void);

#endif // GAMEMAIN_H
