#ifndef ACTOR_323300_H
#define ACTOR_323300_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// Work block of the `actor_323300` enemy task, parked in the task's
/// `Task::idMap` slot (that slot is not a `TaskIdMap` here). The display node
/// at +0x480 is the one the exit callback `func_actor_323300_8016269C` hands
/// back to `Gp_UnlinkObj` before tearing the enemy task down.
typedef struct Actor323300Work {
    /* 0x000 */ byte  pad_0[0x480];
    /* 0x480 */ GpObj obj;
} Actor323300Work;

#endif
