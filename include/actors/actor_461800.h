#ifndef ACTOR_461800_H
#define ACTOR_461800_H

#include "common.h"

#include "main/task.h"

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_461800_80132390` allocates it
/// with `Mem_Calloc(0x4F8, 0)` and stores it into that field (and into
/// `D_actor_461800_80143894`), so the size below is the allocation and not a
/// guess. Reach it with `(Actor461800Work*)task->idMap`.
///
/// The two tasks at +0x4F0 and +0x4F4 are the helper tasks this actor spawns;
/// the exit callback `func_actor_461800_80132A90` kills both on teardown.
typedef struct Actor461800Work {
    /* 0x000 */ byte  pad_0[0x4F0];
    /* 0x4F0 */ Task* field_4F0;
    /* 0x4F4 */ Task* field_4F4;
} Actor461800Work;
STATIC_ASSERT_SIZEOF(Actor461800Work, 0x4F8);

#endif
