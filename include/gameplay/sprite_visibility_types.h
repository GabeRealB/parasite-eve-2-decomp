#ifndef GAMEPLAY_SPRITE_VISIBILITY_TYPES_H
#define GAMEPLAY_SPRITE_VISIBILITY_TYPES_H

#include "common.h"

/// One view-selection, batch-visibility or end command in a sprite visibility stream.
///
/// Streams begin with a view selection, and each selection is immediately
/// followed by a batch write. Slots index an area's `SpriteView` array or the
/// selected view's `SpriteBatch` list, starting at zero. Index 255 ends the
/// stream; its operation byte and any following stored records are unread.
/// Selected views must exist, and their batches must remain writable during decoding.
typedef struct {
    u8 slotIndex; // View or batch slot according to operation; 255 ends the stream
    u8 operation; // Batch visibility (0 visible, 1..254 hidden), or 255 to select a view
} SpriteVisibilityCommand;
STATIC_ASSERT_SIZEOF(SpriteVisibilityCommand, 2);

#endif // GAMEPLAY_SPRITE_VISIBILITY_TYPES_H
