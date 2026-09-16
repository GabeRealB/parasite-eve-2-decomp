#ifndef ACTORS_SHARED_80132ECC_H
#define ACTORS_SHARED_80132ECC_H

#include "common.h"

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc(0x4F8, 0)` and stores it into that field, so
/// the size below is the allocation and not a guess.
///
/// The animation state at 0x4B4..0x4BA is laid out and used exactly as the
/// carriers' own work blocks keep it (see `Actor143900Work`): the reset mode
/// 1 or 2 the play-animation handler latches, the animation id the slot reseed
/// seeds from, its change-detection copy and the flag cleared before the
/// reseed. `actor_143900` is the only carrier that reaches these through this
/// type, its `D_actor_143900_801496C4`.
///
/// The two tasks at +0x4F0 and +0x4F4 are the helper tasks that spawn routine
/// starts; the exit callback kills both on teardown.
typedef struct ActorsShared80132eccWork {
    /* 0x000 */ byte  pad_0[0x4B4];
    /* 0x4B4 */ s16   field_4B4; // reset mode the play-animation handler selects (1 or 2)
    /* 0x4B6 */ s16   field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16   field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16   field_4BA; // cleared by the handler before the reseed
    /* 0x4BC */ byte  pad_4BC[0x34];
    /* 0x4F0 */ Task* field_4F0;
    /* 0x4F4 */ Task* field_4F4;
} ActorsShared80132eccWork;
STATIC_ASSERT_SIZEOF(ActorsShared80132eccWork, 0x4F8);

/// `Task::exitCallback` shared by the actors that spawn a pair of helper
/// tasks: hands the task's `GpEnemy` (parked in `Task::spawnArg2` by the spawn
/// descriptor) back to `Gp_DestroyEnemy`, then kills both helpers.
void ActorsShared80132ecc(Task* task);

#endif
