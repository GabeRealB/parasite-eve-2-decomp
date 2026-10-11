#ifndef MAIN_GAME_DEBUG_H
#define MAIN_GAME_DEBUG_H

#include "main/game_debug_types.h"

/// Read-only byte base of the fixed attract replay in development RAM.
///
/// `DISPLAY_DEMO_FIXED_REPLAY` selects this address instead of actor-load
/// buffer 2. The resource begins with serialized save/player/flag records,
/// followed by native u16 button/duration pairs and a button terminator.
/// The byte view permits the prefix copies and byte-offset cursor limit;
/// only the aligned input stream is viewed as `GameDebugReplayRecord`.
///
/// Playback requires the development host to provide 0x18000 readable bytes
/// at this word-aligned address and keep them intact until playback ends.
/// The macro neither loads nor reserves storage; retail RAM does not reach
/// this address. Evaluating it performs no access and captures no identifiers.
#define GAME_DEBUG_FIXED_REPLAY_BASE ((const u8*)0x80600100)

/// Shared resident diagnostic controls, input overrides and session-loading status.
///
/// Points to fixed resident storage for the program's lifetime. Main-loop
/// initialization clears the complete record; gameplay updates the live
/// controls and status in place. The pointer and its storage survive overlays.
extern GameDebugState* gGameDebugState;

#endif // MAIN_GAME_DEBUG_H
