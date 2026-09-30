#ifndef GAMEPLAY_DIRECTION_H
#define GAMEPLAY_DIRECTION_H

#include "common.h"

#include "gameplay/view.h"
#include "gameplay/message.h"

/// Requests a room-specific action from the current trigger.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows this four-byte, two-byte-aligned
/// record until synchronous dispatch returns. Action IDs belong to the room's
/// handler, with zero a valid ID. The argument is a full byte whose meaning
/// depends on that action, such as a destination area or a task's spawn argument.
typedef struct DirectionActionRequest {
    u16 control;  // Trigger control: low byte 5 selects room dispatch; upper bits are preserved
    u8  actionId; // Room-specific action ID
    u8  argument; // Action-specific byte, interpreted by the selected room handler
} DirectionActionRequest;
STATIC_ASSERT_SIZEOF(DirectionActionRequest, 4);

/// Delivers a borrowed `DirectionActionRequest` with a zero second payload word.
enum {
    DIRECTION_MESSAGE_ROOM_ACTION = 0x13EF,
};

/// 0x38-byte record in tables pointed to by `Gp_WarpTables`. Indexed
/// 1-based by `GameLocationKey.stage` / `area`, then
/// `(Gp_DirNibble >> 4)`. `Gp_CommitWarp` copies one record onto the
/// stack and writes `field_36` into `RoomEventMsg.flagId`. The transform words
/// keep the record 4-aligned for its 56-byte assignment.
/// func_800AA548 uses the transforms at 0x00 / 0x14 to spawn the player /
/// companion, field_28 as a sound event, and field_34 as the initial view.
typedef struct _GpWarpRec {
    /* 0x00 */ GpSpawnTransform player;
    /* 0x10 */ byte             pad_10[4];
    /* 0x14 */ GpSpawnTransform companion;
    /* 0x24 */ byte             pad_24[4];
    /* 0x28 */ s32              field_28;
    /* 0x2C */ s32              field_2C;
    /* 0x30 */ s32              field_30;
    /* 0x34 */ u8               field_34;
    /* 0x35 */ u8               field_35;
    /* 0x36 */ u16              field_36;
} GpWarpRec;
STATIC_ASSERT_SIZEOF(GpWarpRec, 0x38);

#endif // GAMEPLAY_DIRECTION_H
