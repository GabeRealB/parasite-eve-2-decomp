#ifndef INCLUDE_AYA_AYA_20900_H
#define INCLUDE_AYA_AYA_20900_H

#include "main/task_types.h"

/// Indices of the package's bodyless presentation tasks.
enum {
    AYA_20900_TASK_GAME_OVER = 0,
    AYA_20900_TASK_RED_FLASH = 3,
    AYA_20900_TASK_COUNT     = 4,
};

/// Task recipes for the Game Over screen and the red-image/white-flash effect.
///
/// Slots 1 and 2 have no callback. Spawn arguments are unused. Keep this
/// package loaded until its tasks and queued GPU work finish. Game Over
/// requires the logo texture/palette and restart-mode display setup; the red
/// flash requires the resident image strips to be selected for display.
/// Gameplay spawns slot 0 and polls its exit request to finish restart UI.
/// The creation path of slot 3 is unproven.
extern TaskDesc gAya20900TaskDescriptors[AYA_20900_TASK_COUNT];

#endif // INCLUDE_AYA_AYA_20900_H
