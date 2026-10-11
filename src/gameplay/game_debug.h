#ifndef GAMEPLAY_PRIVATE_GAME_DEBUG_H
#define GAMEPLAY_PRIVATE_GAME_DEBUG_H

#include "game_debug_types.h"

/// Button sample cached from the current diagnostic replay record.
///
/// 0xFFFF invalidates the cache so the next update installs a record's duration.
/// A halted update returns the cache unchanged; live Start may augment its output.
extern u16 gGameDebugReplayButtons;

/// Input-update calls left for the cached diagnostic replay sample.
///
/// Initialized to one and replaced by a changed record's duration. Each running
/// call decrements with u16 wraparound; zero advances the cursor and invalidates
/// the cache. A zero stored duration therefore lasts 65536 running calls.
extern u16 gGameDebugReplayFramesRemaining;

/// Borrowed current button/duration pair in the loaded diagnostic replay buffer.
///
/// Points after the 0xD4C-byte serialized prefix in actor buffer 2, or the fixed
/// development buffer, and advances by one four-byte record. Keep the complete
/// 0x18000-byte resource live through replay; no allocation or freeing occurs.
/// Playback checks a signed byte offset, without a lower-bound validation.
extern const GameDebugReplayRecord* gGameDebugReplayCursor;

#endif // GAMEPLAY_PRIVATE_GAME_DEBUG_H
