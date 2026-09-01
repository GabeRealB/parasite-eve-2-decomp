#ifndef ACTORS_SHARED_80132ECC_H
#define ACTORS_SHARED_80132ECC_H

#include "common.h"

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc(0x4F8, 0)` and stores it into that field, so
/// the size below is the allocation and not a guess.
///
/// The two tasks at +0x4F0 and +0x4F4 are the helper tasks that spawn routine
/// starts; the exit callback kills both on teardown.
typedef struct ActorsShared80132eccWork {
    /* 0x000 */ byte  pad_0[0x4F0];
    /* 0x4F0 */ Task* field_4F0;
    /* 0x4F4 */ Task* field_4F4;
} ActorsShared80132eccWork;
STATIC_ASSERT_SIZEOF(ActorsShared80132eccWork, 0x4F8);

/// `Task::exitCallback` shared by the actors that spawn a pair of helper
/// tasks: hands the task's `GpEnemy` (parked in `Task::spawnArg2` by the spawn
/// descriptor) back to `Gp_DestroyEnemy`, then kills both helpers.
void ActorsShared80132ecc(Task* task);

#endif
