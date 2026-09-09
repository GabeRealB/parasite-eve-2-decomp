#ifndef ACTORS_SHARED_80132710_H
#define ACTORS_SHARED_80132710_H

#include "common.h"

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate
/// different total sizes, so only the one field this body reaches is described
/// rather than a whole-block size that would be wrong for some of them.
///
/// `field_4B8` is the task of the sibling this actor was paired with when it
/// spawned; this body drives that task's model alongside its own.
typedef struct ActorsShared80132710Work {
    /* 0x000 */ byte  pad_0[0x4B8];
    /* 0x4B8 */ Task* field_4B8;
} ActorsShared80132710Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132710Work, 0x4BC);

/// Script opcode: set the visibility flags of this actor's model and of the
/// model owned by the sibling task at `field_4B8`.
s32 ActorsShared80132710(Task* task, s32 arg1, s32 flags);

#endif
