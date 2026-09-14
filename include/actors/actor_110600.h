#ifndef ACTOR_110600_H
#define ACTOR_110600_H

#include "common.h"

#include "main/task.h"

/// Work block this overlay parks in the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. `field_0` is the state index
/// `func_actor_110600_801387C0` writes.
typedef struct Actor110600Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ byte pad_2[0xBE2];
    /* 0xBE4 */ s16  field_BE4;
    /* 0xBE6 */ s16  field_BE6;
} Actor110600Work;

s32 func_actor_110600_801387C0(Task* arg0);

#endif
