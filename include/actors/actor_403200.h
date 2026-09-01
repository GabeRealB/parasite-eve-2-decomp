#ifndef ACTOR_403200_H
#define ACTOR_403200_H

#include "common.h"
#include "main/task.h"

typedef struct Actor403200Obj Actor403200Obj;

/// Per-actor state block for the `actor_403200` overlay.
///
/// `func_actor_403200_80138AFC` allocates it with `Mem_Calloc(0xF24, 0)` and
/// stores the result in the `Task::idMap` slot (0x1C), which this enemy actor
/// reuses for its own work block, so it is *not* a `TaskIdMap` here. Reach it
/// with `(Actor403200Work*)task->idMap`. The size below is the allocation, not
/// a guess.
typedef struct Actor403200Work {
    /* 0x000 */ byte pad_0[0xEAC];
    /* 0xEAC */ s8   field_EAC;
    /* 0xEAD */ byte pad_EAD[0x77];
} Actor403200Work;
STATIC_ASSERT_SIZEOF(Actor403200Work, 0xF24);

s16 func_actor_403200_801344C4(Actor403200Obj* arg0, s16 arg1);

#endif
