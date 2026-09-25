#ifndef ACTOR_205200_H
#define ACTOR_205200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

typedef struct Actor205200Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor205200Obj2C;

/// The task itself, as the actor's three kinds of task - the controller, its
/// parts and the actor proper - see it. `field_8` is `Task::parent`: a part's
/// parent is the controller. `field_1C` is the task's work block, whose type
/// depends on which of the three the task is.
typedef struct Actor205200 {
    /* 0x00 */ byte              pad_0[0x8];
    /* 0x08 */ struct Task*      field_8;
    /* 0x0C */ byte              pad_C[0x10];
    /* 0x1C */ void*             field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ byte              pad_24[0x8];
    /* 0x2C */ Actor205200Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor205200;

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// its halfword at 0x2 is the only part the handlers read. Senders seed it
/// from a `Task`'s `spawnArg1` halfword -- `Gp_DispatchMsg` in `3CD8.c` sends
/// `D_801155A0`.
typedef struct Actor205200Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor205200Msg7DB;
STATIC_ASSERT_SIZEOF(Actor205200Msg7DB, 0x4);

#endif
