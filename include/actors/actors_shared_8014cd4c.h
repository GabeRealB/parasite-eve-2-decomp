#ifndef ACTORS_SHARED_8014CD4C_H
#define ACTORS_SHARED_8014CD4C_H

#include "common.h"

#include "main/task.h"

/// Work block the carriers hang off the task's `Task::work` slot (0x1C), which
/// is not a `TaskIdMap` here. The carriers allocate different total sizes, so
/// only the one field this body reaches is described rather than a whole-block
/// size that would be wrong for some of them.
///
/// `field_4F0` is the task of the sibling this actor was paired with when it
/// spawned; this body drives that task's model alongside its own.
typedef struct ActorsShared8014cd4cWork {
    /* 0x000 */ byte  pad_0[0x4F0];
    /* 0x4F0 */ Task* field_4F0;
} ActorsShared8014cd4cWork;
STATIC_ASSERT_SIZEOF(ActorsShared8014cd4cWork, 0x4F4);

/// Script opcode: set the visibility flags of this actor's model and of the
/// model owned by the sibling task at `field_4F0`. Shared by `actor_160700`
/// and `actor_215100`.
s32 ActorsShared8014cd4c(Task* task, s32 arg1, s32 flags);

#endif
