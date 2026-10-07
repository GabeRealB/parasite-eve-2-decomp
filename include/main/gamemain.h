#ifndef MAIN_GAMEMAIN_H
#define MAIN_GAMEMAIN_H

#include "types.h"

extern volatile s32 GameMain_HaltFlags;

/// Returns the number of resident game initializations since startup.
///
/// Includes the first boot initialization (count 1) and each subsequent game
/// restart or accepted soft reset. The u32 count wraps on overflow; reading it
/// changes no state. The title uses it to rotate attract-mode demo scenes.
u32 gameMainGetInitializationCount(void);

#endif // MAIN_GAMEMAIN_H
