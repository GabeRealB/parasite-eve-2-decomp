#ifndef GAMEPLAY_MESSAGE_H
#define GAMEPLAY_MESSAGE_H

#include "common.h"

#include <psyq/libgte.h>

// The records carried as the payload of task messages. A sender passes a
// pointer to one alongside the message id, and every task that answers the
// message reads the same layout, whichever overlay it lives in. Event scripts
// keep these records as data and send them as they run.

/// A position and a set of Euler angles, the payload of the messages that put
/// a task somewhere. The player takes it to be placed or warped, and as the
/// point to walk to; the actors take it to be placed, and as the point to walk
/// to or turn towards, where some read only the position.
typedef struct GpXformArg {
    VECTOR  pos;
    SVECTOR rot;
} GpXformArg;
STATIC_ASSERT_SIZEOF(GpXformArg, 0x18);

/// The payload of the messages that start an animation. `animBlock` picks the
/// animation block to play from: the player's and the actors' plain play
/// messages take an `index` into the receiver's own table of blocks and
/// reinstall the block only when it changes, while the player's messages that
/// install a block directly take the block itself as `ptr`. Event scripts that
/// address the player or the companion have the index rewritten to the one for
/// the equipped weapon or the current companion before the message is sent.
/// `field_4` is the animation within the block. With `field_8` zero the
/// animation slots are reset to it; otherwise it is blended in, and `field_C`
/// is handed to the blend. The player's direct-install messages also read
/// `field_10`, which selects between two display modes.
typedef struct GpAnimArg {
    union {
        s32   index;
        void* ptr;
    } animBlock;
    s32 field_4;
    s32 field_8;
    s32 field_C;
    s32 field_10;
} GpAnimArg;
STATIC_ASSERT_SIZEOF(GpAnimArg, 0x14);

#endif
