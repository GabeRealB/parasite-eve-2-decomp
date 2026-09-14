#ifndef ACTOR_207000_H
#define ACTOR_207000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Object stored in `Task::extra` (0x2C). `field_8` is the coordinate chain the
/// overlay's spawn routines read: `field_8[0]` gives the position the spawned
/// task is placed at (through its `workm.t`), and `field_8[1]` is the
/// coordinate whose matrix the new task inherits.
typedef struct Actor207000Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor207000Obj2C;

typedef struct Actor207000 {
    /* 0x00 */ byte              pad_0[0x2C];
    /* 0x2C */ Actor207000Obj2C* field_2C;
} Actor207000;

s32 func_actor_207000_8014E614(GsCOORDINATE2* arg0, s32* arg1);

#endif // ACTOR_207000_H
