#ifndef ACTOR_101900_H
#define ACTOR_101900_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// Private work block of the actor 01900 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` list nodes unlinked by the
/// destroy callback and the two child tasks it kills. `Actor01900_Fn0A764`
/// masks `field_A08.flags` and `field_B48.flags`, which is what fixes those
/// two offsets as `GpObj` rather than opaque padding.
typedef struct Actor01900Work {
    /* 0x000 */ s16   field_0;
    /* 0x002 */ s16   field_2;
    /* 0x004 */ byte  pad_4[0x89A];
    /* 0x89E */ s16   field_89E;
    /* 0x8A0 */ byte  pad_8A0[0x28];
    /* 0x8C8 */ GpObj field_8C8;
    /* 0x8E8 */ byte  pad_8E8[0x120];
    /* 0xA08 */ GpObj field_A08;
    /* 0xA28 */ byte  pad_A28[0x120];
    /* 0xB48 */ GpObj field_B48;
    /* 0xB68 */ byte  pad_B68[0xD0];
    /* 0xC38 */ Task* field_C38;
    /* 0xC3C */ Task* field_C3C;
} Actor01900Work;

/// Per-task actor context handed to the overlay's callbacks: `field_1C` is the
/// work block above (the same pointer `Task::idMap` holds) and `field_2C` is
/// the actor's `TmdObject`. Same shape as the other actor overlays' contexts.
typedef struct Actor01900 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor01900Work* field_1C;
    /* 0x20 */ byte            pad_20[0xC];
    /* 0x2C */ TmdObject*      field_2C;
} Actor01900;

/// Per-room clamp applied to the accumulated height offset of the actor's
/// motion scratch. `field_0` / `field_2` are matched against
/// `GameSession.field_7` / `field_6`, and when a row matches the offset is
/// clamped to [`lo`, `hi`]. `Actor01900_D172CC` holds two live rows plus a
/// zero terminator row that the two-iteration scan never reaches.
typedef struct Actor01900HeightClamp {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  lo;
    /* 0x6 */ s16  hi;
    /* 0x8 */ byte pad_8[8];
} Actor01900HeightClamp;
STATIC_ASSERT_SIZEOF(Actor01900HeightClamp, 0x10);

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `Actor01900_Fn03C98`.
/// The first 0x10 bytes are the `GpDeltaScratch` passed to `func_800E0C10`;
/// `field_1C` is the running height offset that `Actor01900_Fn03C04` clamps
/// against the current room's `Actor01900_D172CC` row.
typedef struct Actor01900Delta {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ byte           pad_10[0xC];
    /* 0x1C */ s32            field_1C;
} Actor01900Delta;
STATIC_ASSERT_SIZEOF(Actor01900Delta, 0x20);

/// Payload of the `0x7D3` message the overlay's `Actor01900_D1728C` handler
/// table dispatches to `Actor01900_Fn0A31C`. Senders build the record in their
/// own data (`D_actor_146300_80137AAC` and friends); `field_4` selects which
/// animation id the actor switches to.
typedef struct Actor01900Msg7D3 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor01900Msg7D3;

extern Actor01900HeightClamp Actor01900_D172CC[];

void Actor01900_Fn03C04(GameSessionFrom4* session, Actor01900Delta* delta);
s32  Actor01900_Fn0A31C(Actor01900* arg0, s32 arg1, Actor01900Msg7D3* arg2);
s32  Actor01900_Fn0A38C(Actor01900* arg0, s32 arg1, s32 arg2);

#endif
