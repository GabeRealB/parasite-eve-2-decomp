#ifndef ACTORS_SHARED_80168D3C_H
#define ACTORS_SHARED_80168D3C_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// 0x454-byte work block `actor_341700` and `actor_342400` allocate with
/// `Mem_Calloc` and park in `Task::idMap` (that slot is not a `TaskIdMap`
/// here). `field_92` is the low half of the model root `coord.t[1]`,
/// `obj_2AC` / `obj_2CC` / `obj_3AC` the `Gp_LinkObj` nodes the death handler
/// unlinks, `field_412` the per-state frame counter, `field_422` the sub-state
/// index the handler table walks, and `field_414` .. `field_426` the animation
/// request the actor hands to its player. The size below is the allocation,
/// not a guess.
typedef struct ActorsShared80168d3cWork {
    /* 0x000 */ byte    pad_0[0x92];
    /* 0x092 */ u16     field_92; // low half of root coord.t[1]
    /* 0x094 */ byte    pad_94[0x218];
    /* 0x2AC */ GpObj   obj_2AC;
    /* 0x2CC */ GpObj   obj_2CC;
    /* 0x2EC */ GpRec18 rec_2EC[8];
    /* 0x3AC */ GpObj   obj_3AC;
    /* 0x3CC */ byte    pad_3CC[0x46];
    /* 0x412 */ u16     field_412; // per-state frame counter
    /* 0x414 */ s16     field_414; // animation request kind
    /* 0x416 */ byte    pad_416[0x2];
    /* 0x418 */ s16     field_418; // animation id
    /* 0x41A */ byte    pad_41A[0x2];
    /* 0x41C */ s16     field_41C; // animation speed / step scale
    /* 0x41E */ byte    pad_41E[0x2];
    /* 0x420 */ u16     field_420; // state index
    /* 0x422 */ u16     field_422; // sub-state index
    /* 0x424 */ byte    pad_424[0x2];
    /* 0x426 */ s16     field_426;
    /* 0x428 */ s16     field_428;
    /* 0x42A */ s16     field_42A;
    /* 0x42C */ byte    pad_42C[0x6];
    /* 0x432 */ s16     field_432;
    /* 0x434 */ byte    pad_434[0x4];
    /* 0x438 */ s16     field_438;
    /* 0x43A */ byte    pad_43A[0x6];
    /* 0x440 */ s16     field_440;
    /* 0x442 */ byte    pad_442[0x12];
} ActorsShared80168d3cWork;
STATIC_ASSERT_SIZEOF(ActorsShared80168d3cWork, 0x454);

/// Seed the next sub-state from the model root Y, request animation 8 at
/// speed 0x10, clear the frame counter and two motion halfwords, and advance
/// `field_422`. `Actor04400_Fn06EEC` is the same body inside the slot-1/slot-3
/// `actor_104400_text` object, which cannot join this unit: that overlay's
/// whole `.text` is already one shared span.
void ActorsShared80168d3c(Task* arg0);

#endif
