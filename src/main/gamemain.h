#ifndef MAIN_PRIVATE_GAMEMAIN_H
#define MAIN_PRIVATE_GAMEMAIN_H

#include "types.h"

/// VSync wait mode (0 next blank, 2 two blanks, 3 three blanks); also drives play-clock pacing.
extern s32 D_8005EC68;

/// Frame budget in VSync(1) horizontal-line counts, separate from animation ticks.
extern s32 D_8005EC6C;

extern volatile s32 Display_PendingFlip;

/// Written by _displayVSyncCallback; read by Display_FrameFlipDraw.
extern volatile s32 D_8005EC74;

/// Cleared/set by the draw path; read by the VSync callback for lag accounting.
extern volatile s32 D_8005EC78;

/// Game entry point. Called by `main`.
void GameMain(void);

#endif // MAIN_PRIVATE_GAMEMAIN_H
