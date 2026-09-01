#ifndef ACTOR_260400_H
#define ACTOR_260400_H

#include "common.h"

#include "main/task.h"

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_260400_80149FE0` allocates it
/// with `Mem_Calloc(0x4F8, 0)` and stores it straight into that field, so the
/// size below is the allocation and not a guess. Reach it with
/// `(Actor260400Work*)task->idMap`.
///
/// The task at +0x4F0 is the helper task this actor spawns; the exit callback
/// `func_actor_260400_8014A630` kills it on teardown.
typedef struct Actor260400Work {
    /* 0x000 */ byte  pad_0[0x4F0];
    /* 0x4F0 */ Task* field_4F0;
    /* 0x4F4 */ byte  pad_4F4[0x4];
} Actor260400Work;
STATIC_ASSERT_SIZEOF(Actor260400Work, 0x4F8);

#endif
