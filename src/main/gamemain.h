#ifndef MAIN_PRIVATE_GAMEMAIN_H
#define MAIN_PRIVATE_GAMEMAIN_H

#include "types.h"

// Display/CD timing flags shared with the VSync path (GameMain_ShowLoading / GameMain_Loop).
extern s32 D_8005EC68;

extern s32 D_8005EC6C;

extern volatile s32 Display_PendingFlip;

/// Written by Display_VSyncCallback; read by Display_FrameFlipDraw.
extern volatile s32 D_8005EC74;

/// Cleared/set by the draw path; read by the VSync callback for lag accounting.
extern volatile s32 D_8005EC78;

/// Game entry point. Called by `main`.
void GameMain(void);

#endif // MAIN_PRIVATE_GAMEMAIN_H
