#ifndef ACTOR_205200_H
#define ACTOR_205200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"

typedef struct Actor205200Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor205200Obj2C;

/// Work block this overlay hangs off `Actor205200.field_1C` (the task's
/// `Task::idMap` slot, which is not a `TaskIdMap` here). The two display nodes
/// at +0x47C and +0x4E4 are the ones the exit callback
/// `func_actor_205200_8014C924` hands back to `Gp_UnlinkObj`.
typedef struct Actor205200Work {
    /* 0x000 */ byte  pad_0[0x47C];
    /* 0x47C */ GpObj field_47C;
    /* 0x49C */ byte  pad_49C[0x48];
    /* 0x4E4 */ GpObj field_4E4;
    /* 0x504 */ byte  pad_504[0x90];
    /* 0x594 */ s16   field_594;
} Actor205200Work;

typedef struct Actor205200 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor205200Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor205200Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor205200;

/// Owning context. The update entry point does not touch it, but the exit
/// callback `func_actor_205200_8014C924` unlinks the `GpLinkNode` at +0x10.
typedef struct Actor205200Ctx {
    /* 0x00 */ byte       pad_0[0x10];
    /* 0x10 */ GpLinkNode node;
} Actor205200Ctx;

void func_actor_205200_8014C59C(Actor205200Ctx* arg0, Actor205200* arg1);
void func_actor_205200_8014C924(Actor205200Ctx* arg0, Actor205200* arg1);

#endif
