#ifndef GAMEPLAY_PRIVATE_GAME_DEBUG_TYPES_H
#define GAMEPLAY_PRIVATE_GAME_DEBUG_TYPES_H

#include "common.h"

/// One native-halfword diagnostic replay record, after the serialized save prefix.
///
/// Records advance by four bytes; a 0xFFFF buttons halfword ends the stream.
/// The buffer owner keeps the current pair and the next button readable during
/// playback. Duration is counted in input-update calls with u16 wraparound.
typedef struct {
    u16 buttons;        // Active-high pad buttons; 0xFFFF is the end marker.
    u16 durationFrames; // Calls for which the button sample is held.
} GameDebugReplayRecord;
STATIC_ASSERT_SIZEOF(GameDebugReplayRecord, 4);

#endif // GAMEPLAY_PRIVATE_GAME_DEBUG_TYPES_H
