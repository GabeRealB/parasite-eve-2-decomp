#ifndef ACTOR_403600_H
#define ACTOR_403600_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

#include "gameplay/3A34.h"

/// Work block of the `actor_403600` task, parked in the task's `Task::idMap`
/// slot (that slot is not a `TaskIdMap` here). The display node at +0x108 is
/// the one the exit callback `func_actor_403600_80138C68` hands back to
/// `Gp_UnlinkObj`.
typedef struct Actor403600Work {
    /* 0x000 */ byte          pad_0[0x108];
    /* 0x108 */ GpObj         obj;
    /* 0x128 */ byte          pad_128[0x390];
    /* 0x4B8 */ GsCOORDINATE2 field_4B8;
    /* 0x508 */ GpObj         field_508;
    /* 0x528 */ byte          pad_528[0x60];
    /* 0x588 */ GpObj         field_588;
    /* 0x5A8 */ byte          pad_5A8[0x18];
    /* 0x5C0 */ GpObj         field_5C0;
    /* 0x5E0 */ byte          pad_5E0[0x128];
    /* 0x708 */ s16           field_708;
    /* 0x70A */ byte          pad_70A[0x26];
    /* 0x730 */ s16           field_730;
    /* 0x732 */ byte          pad_732[0x10];
    /* 0x742 */ s16           field_742;
    /* 0x744 */ byte          pad_744[0x36];
    /* 0x77A */ s16           field_77A;
    /* 0x77C */ byte          pad_77C[0x30];
    /* 0x7AC */ s16           field_7AC;
} Actor403600Work;

typedef struct Actor403600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor403600Work* field_1C;
    /* 0x20 */ struct _GpEnemy* field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor403600;

typedef struct Actor403600Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor403600Ctx;

void ActorsShared80131e24Sub1(Actor403600Ctx* arg0, Actor403600* arg1);

#endif
