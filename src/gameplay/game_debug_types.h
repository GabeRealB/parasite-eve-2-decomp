#ifndef GAMEPLAY_PRIVATE_GAME_DEBUG_TYPES_H
#define GAMEPLAY_PRIVATE_GAME_DEBUG_TYPES_H

#include "common.h"

/// One button hold in the diagnostic input replay stream.
///
/// The stream follows the serialized saved state and stores little-endian u16
/// button/hold-count pairs at a four-byte stride. Holds count running replay
/// input updates, independent of elapsed display ticks: halted updates do not
/// consume them, and zero lasts 65536 updates through u16 wrap.
/// A trailing 0xFFFF button halfword ends playback without reading its hold
/// count.
///
/// Records borrow halfword-aligned resource storage. Its owner must keep each
/// current pair and the next button halfword readable throughout playback.
typedef struct {
    u16 buttons;     // Active-high PAD_BUTTON_* mask; 0xFFFF marks the end of the stream.
    u16 holdUpdates; // Running input updates to hold the mask (1..65535; 0 means 65536).
} GameDebugReplayRecord;
STATIC_ASSERT_SIZEOF(GameDebugReplayRecord, 4);

#endif // GAMEPLAY_PRIVATE_GAME_DEBUG_TYPES_H
