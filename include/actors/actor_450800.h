#ifndef ACTOR_450800_H
#define ACTOR_450800_H

#include "common.h"

#include "main/task.h"

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_450800_80132160` allocates it
/// with `Mem_Calloc(0x504, 0)` and stores it straight into that field, so the
/// size below is the allocation and not a guess. Reach it with
/// `(Actor450800Work*)task->idMap`.
///
/// The three tasks at +0x4F0 .. +0x4F8 are the helper tasks this actor spawns;
/// the exit callback `func_actor_450800_80132868` kills all three on teardown.
typedef struct Actor450800Work {
    /* 0x000 */ byte  pad_0[0x4F0];
    /* 0x4F0 */ Task* field_4F0;
    /* 0x4F4 */ Task* field_4F4;
    /* 0x4F8 */ Task* field_4F8;
    /* 0x4FC */ byte  pad_4FC[0x8];
} Actor450800Work;
STATIC_ASSERT_SIZEOF(Actor450800Work, 0x504);

#endif
