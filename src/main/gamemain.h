#ifndef MAIN_PRIVATE_GAMEMAIN_H
#define MAIN_PRIVATE_GAMEMAIN_H

#include "types.h"

/// Initializes resident drivers once and runs the game until power-off.
///
/// Call once with the CPU stack established and no live tasks or allocations.
/// Selects NTSC, initializes sound, memory cards, controllers and CD access,
/// clears the state retained across soft resets, then initializes the first
/// game run. Never returns; subsequent resets rebuild the run inside the loop
/// without repeating this driver setup or clearing the persistent state.
void gameMainRun(void);

#endif // MAIN_PRIVATE_GAMEMAIN_H
