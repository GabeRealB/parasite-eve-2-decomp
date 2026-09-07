#ifndef ACTORS_SHARED_8016A074_H
#define ACTORS_SHARED_8016A074_H

#include "common.h"

#include "main/task.h"

/// Work block the carriers of `ActorsShared8016a074` park in the task's
/// `Task::idMap` slot (0x1C), which is not a `TaskIdMap` here. `field_0` is the
/// actor's state id.
typedef struct ActorsShared8016a074Work {
    /* 0x00 */ s16 field_0;
} ActorsShared8016a074Work;

s32 ActorsShared8016a074(Task* task);

#endif
