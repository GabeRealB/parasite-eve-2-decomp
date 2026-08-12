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

void GameMain_Init(void);
void GameMain_Loop(void);
u32  GameMain_GetResetCount(void);
void GameMain_ShowLoading(s32 arg0);
void GameMain_SetFrameTiming(s32 arg0);
void GameMain_SpawnBootTask(void);

/// "PAUSE!" overlay text for GameMain_ShowLoading (@ VA 0x80013404).
extern const u8 GameMain_PauseText[];

// Display/CD timing flags shared with the VSync path (GameMain_ShowLoading / GameMain_Loop).
extern u32          D_8005EC64;
extern s32          D_8005EC68;
extern s32          D_8005EC6C;
extern volatile u32 D_8005EC70;
/// Written by Display_VSyncCallback; read by Display_FrameFlipDraw.
extern volatile s32 D_8005EC74;
/// Cleared/set by the draw path; read by the VSync callback for lag accounting.
extern volatile s32 D_8005EC78;
extern volatile s32 D_8005EC80;

/// Immediate-mode TILE / DR_TPAGE scratch for the "now loading" overlay.
extern TILE     D_8006EC18;
extern DR_TPAGE D_8006EC28;

#endif // GAMEMAIN_H
