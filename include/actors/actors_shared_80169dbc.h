#ifndef ACTORS_SHARED_80169DBC_H
#define ACTORS_SHARED_80169DBC_H

#include "common.h"

#include "main/task.h"

/// 0x454-byte work block `actor_341700` and `actor_342400` allocate with
/// `Mem_Calloc` and park in `Task::idMap` (that slot is not a `TaskIdMap`
/// here). `field_412` is the per-state frame counter, `field_422` the
/// sub-state index the handler table walks, and `field_414` .. `field_426` the
/// animation request the actor hands to its player. The size below is the
/// allocation, not a guess.
typedef struct ActorsShared80169dbcWork {
    /* 0x000 */ byte pad_0[0x412];
    /* 0x412 */ u16  field_412; // per-state frame counter
    /* 0x414 */ s16  field_414; // animation request kind
    /* 0x416 */ byte pad_416[0x2];
    /* 0x418 */ s16  field_418; // animation id
    /* 0x41A */ byte pad_41A[0x2];
    /* 0x41C */ s16  field_41C; // animation speed / step scale
    /* 0x41E */ byte pad_41E[0x2];
    /* 0x420 */ byte pad_420[0x2];
    /* 0x422 */ u16  field_422; // sub-state index
    /* 0x424 */ byte pad_424[0x2];
    /* 0x426 */ s16  field_426;
    /* 0x428 */ byte pad_428[0x1E];
    /* 0x446 */ s16  field_446; // randomised hold, 0x60 .. 0x9F frames
    /* 0x448 */ byte pad_448[0xC];
} ActorsShared80169dbcWork;
STATIC_ASSERT_SIZEOF(ActorsShared80169dbcWork, 0x454);

/// Start the actor's next sub-state: request animation 1 at speed 0x10, roll a
/// 0x60..0x9F frame hold out of the global LCG, reset the frame counter and
/// advance `field_422`. `Actor04400_Fn07F6C` is the same body inside the
/// slot-1/slot-3 `actor_104400_text` object, which cannot join this unit: that
/// overlay's whole `.text` is already one shared span.
void ActorsShared80169dbc(Task* arg0);

#endif
