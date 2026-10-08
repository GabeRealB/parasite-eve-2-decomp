#ifndef MAIN_PRIVATE_GAMEMAIN_H
#define MAIN_PRIVATE_GAMEMAIN_H

#include "types.h"

/// VSync wait mode (0 next blank, 2 two blanks, 3 three blanks); also drives play-clock pacing.
extern s32 D_8005EC68;

/// Frame budget in VSync(1) horizontal-line counts, separate from animation ticks.
extern s32 D_8005EC6C;

extern volatile s32 Display_PendingFlip;

/// Written by _displayVSyncCallback; read by displayRunTaskFrame.
extern volatile s32 D_8005EC74;

/// Cleared/set by the draw path; read by the VSync callback for lag accounting.
extern volatile s32 D_8005EC78;

/// Initializes resident drivers once and runs the game until power-off.
///
/// Call once with the CPU stack established and no live tasks or allocations.
/// Selects NTSC, initializes sound, memory cards, controllers and CD access,
/// clears the state retained across soft resets, then initializes the first
/// game run. Never returns; subsequent resets rebuild the run inside the loop
/// without repeating this driver setup or clearing the persistent state.
void gameMainRun(void);

#endif // MAIN_PRIVATE_GAMEMAIN_H
