#ifndef ACTOR_341700_H
#define ACTOR_341700_H

#include "common.h"
#include "main/tmd.h"
#include "gameplay/3A34.h"

typedef struct Actor341700 {
    /* 0x00 */ byte       pad_0[0x2C];
    /* 0x2C */ TmdObject* field_2C;
} Actor341700;

/// Per-actor state block for the `actor_341700` overlay's main enemy.
///
/// `func_actor_341700_80162974` and `func_actor_341700_80162B8C` both allocate
/// it with `Mem_Calloc(0x454, 0)` and store it in the `Task::idMap` slot
/// (0x1C), so the size below is the allocation, not a guess: this actor reuses
/// that pointer field for its own work block and it is *not* a `TaskIdMap`
/// here. Reach it with `(Actor341700Work*)task->idMap`.
///
/// `field_420` / `field_422` are the state and sub-state indices the handler
/// table walks; `field_412` is the per-state frame counter. `field_414` ..
/// `field_426` are the animation request the actor hands to its player.
typedef struct Actor341700Work {
    /* 0x000 */ byte  pad_0[0x2AC];
    /* 0x2AC */ GpObj obj_2AC;
    /* 0x2CC */ GpObj obj_2CC;
    /* 0x2EC */ byte  pad_2EC[0xC0];
    /* 0x3AC */ GpObj obj_3AC;
    /* 0x3CC */ byte  pad_3CC[0x46];
    /* 0x412 */ u16   field_412; // per-state frame counter
    /* 0x414 */ s16   field_414; // animation request kind
    /* 0x416 */ byte  pad_416[0x2];
    /* 0x418 */ s16   field_418; // animation id
    /* 0x41A */ byte  pad_41A[0x2];
    /* 0x41C */ s16   field_41C; // animation speed / step scale
    /* 0x41E */ byte  pad_41E[0x2];
    /* 0x420 */ u16   field_420; // state index
    /* 0x422 */ u16   field_422; // sub-state index
    /* 0x424 */ byte  pad_424[0x2];
    /* 0x426 */ s16   field_426;
    /* 0x428 */ byte  pad_428[0xA];
    /* 0x432 */ s16   field_432;
    /* 0x434 */ byte  pad_434[0x4];
    /* 0x438 */ s16   field_438;
    /* 0x43A */ byte  pad_43A[0xE];
    /* 0x448 */ s16   field_448;
    /* 0x44A */ byte  pad_44A[0x5];
    /* 0x44F */ u8    field_44F;
    /* 0x450 */ byte  pad_450[0x1];
    /* 0x451 */ s8    field_451;
    /* 0x452 */ byte  pad_452[0x2];
} Actor341700Work;
STATIC_ASSERT_SIZEOF(Actor341700Work, 0x454);

/// The overlay's *other* work block, for the task `func_actor_341700_8016D130`
/// starts: that function calls `Mem_Calloc(0x80, 0)` and stores the result in
/// the same `Task::idMap` slot, so the two blocks never coexist on one task.
/// Only the leading halfwords are reached from decompiled code so far.
typedef struct Actor341700SubWork {
    /* 0x00 */ s16  field_0;
    /* 0x02 */ s16  field_2;
    /* 0x04 */ s16  field_4;
    /* 0x06 */ byte pad_6[0x7A];
} Actor341700SubWork;
STATIC_ASSERT_SIZEOF(Actor341700SubWork, 0x80);

s32 func_actor_341700_8016CE28(Actor341700* arg0, s32 arg1, s32 arg2);

#endif
