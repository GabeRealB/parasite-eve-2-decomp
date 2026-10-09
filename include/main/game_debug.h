#ifndef MAIN_GAME_DEBUG_H
#define MAIN_GAME_DEBUG_H

#include "main/game_debug_types.h"

/// Shared resident diagnostic controls, input overrides and session-loading status.
///
/// Points to fixed resident storage for the program's lifetime. Main-loop
/// initialization clears the complete record; gameplay updates the live
/// controls and status in place. The pointer and its storage survive overlays.
extern GameDebugState* gGameDebugState;

#endif // MAIN_GAME_DEBUG_H
