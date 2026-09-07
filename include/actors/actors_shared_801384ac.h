#ifndef ACTORS_SHARED_801384AC_H
#define ACTORS_SHARED_801384AC_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// 0xBCC-byte work block this enemy's spawn function allocates with
/// `Mem_Calloc` and parks in the task's `Task::idMap` slot (that slot is not a
/// `TaskIdMap` here). The only part `ActorsShared801384ac` needs is the run of
/// four `GpObj` display nodes at 0x9A8, which it hands back to `Gp_UnlinkObj`.
typedef struct ActorShared801384acWork {
    /* 0x000 */ byte  pad_0[0x9A8];
    /* 0x9A8 */ GpObj field_9A8[4];
    /* 0xA28 */ byte  pad_A28[0x1A4];
} ActorShared801384acWork;
STATIC_ASSERT_SIZEOF(ActorShared801384acWork, 0xbcc);

void ActorsShared801384ac(Task* task);

#endif
