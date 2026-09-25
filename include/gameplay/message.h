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

/// The payload of the message that copies animation parameters onto a
/// task's current animation block: `count` words from `words`, at most 0x20.
typedef struct GpCopyArg {
    s32* words;
    s32  count;
} GpCopyArg;
STATIC_ASSERT_SIZEOF(GpCopyArg, 8);

/// The payload of the message that holds the player or the companion in a
/// timed state. The receivers read only `field_14`, a frame count they store as
/// the state's countdown; senders also fill `field_4`.
typedef struct GpDelayArg {
    byte pad_0[4];
    s32  field_4;
    byte pad_8[0xC];
    s32  field_14;
} GpDelayArg;
STATIC_ASSERT_SIZEOF(GpDelayArg, 0x18);

/// A command to an actor, the payload of message 0x7DB. A sender hands it to
/// one actor directly, or as message 0x7DA to the actor manager in pointer
/// slot 4, which passes it on to its actors as 0x7DB. `from` says who the
/// command is from, usually the stage and area of the room sending it; a
/// receiver tests the two bytes together as one halfword before it acts on
/// `command`.
typedef struct GpCmdArg {
    union {
        struct {
            u8 stage;
            u8 area;
        } loc;
        u16 key; // `loc` read as one halfword, area in the high byte
    } from;
    u16 command; // What the receiver is to do: a state to enter or a request number
} GpCmdArg;
STATIC_ASSERT_SIZEOF(GpCmdArg, 4);

/// The same 0x7DB record as `GpCmdArg`, as the event script runner sends it
/// and a receiver that answers it sees it: `key` is the id the runner looked
/// its target up by, and the halfword `GpCmdArg` reads as `command` is two
/// bytes here. The receiver sets `done` once it has carried the request out,
/// which the runner waits for; the runner sets `field_3` with each request.
///
/// It stays a type of its own rather than a union inside `GpCmdArg` because a
/// union at `command` changes how the code that builds a `GpCmdArg` in place
/// schedule its stores.
typedef struct GpCmdReply {
    u16 key;
    s8  done;
    s8  field_3;
} GpCmdReply;
STATIC_ASSERT_SIZEOF(GpCmdReply, 4);

/// The payload of message 0x3FE, which moves the receiver by a displacement:
/// `x`, `y` and `z` are added onto its coordinate. With `field_10` 7 the move
/// also decides whether the receiver faces along it or away from it, from
/// `x` / `z`; the receiver keeps `field_10` in its own state either way.
/// `field_12` zero first resets the receiver's movement state, so a sender
/// moving it over several frames sets it after the first.
typedef struct GpMoveArg {
    s32  x;
    s32  y;
    s32  z;
    byte pad_C[4];
    s16  field_10;
    u8   field_12;
    byte pad_13;
} GpMoveArg;
STATIC_ASSERT_SIZEOF(GpMoveArg, 0x14);

/// The payload of message 0x3EF, which stops the receiver where it is and
/// plays one of two animations: `field_0` non-zero picks the second. The
/// receiver keeps both words in its own state; senders fill them as two words.
typedef struct GpFacingArg {
    s32 field_0;
    s32 field_4;
} GpFacingArg;
STATIC_ASSERT_SIZEOF(GpFacingArg, 8);

/// The optional second payload of message 0x3F2, which sends the receiver to a
/// `GpXformArg` destination: two values the receiver keeps in its own state
/// while it gets there. Without one it clears both.
typedef struct GpOverrideArg {
    s32 field_0;
    s32 field_4;
} GpOverrideArg;
STATIC_ASSERT_SIZEOF(GpOverrideArg, 8);

#endif
