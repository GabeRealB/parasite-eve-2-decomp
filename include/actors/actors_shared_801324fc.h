#ifndef ACTORS_SHARED_801324FC_H
#define ACTORS_SHARED_801324FC_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// The part of the `ActorsShared80131f9c` work block this body touches. Each
/// carrier's block is its own type (see `include/actors/actor_151000.h` for
/// the full 0x4C0 layout); the shared unit names only the animation context,
/// the slot array behind it and the record this handler latches.
typedef struct ActorsShared801324fcWork {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       pad_34C[0x16C];
    /* 0x4B8 */ GpAnimRec* field_4B8; // last `Gp_AnimGetRec` result, to spot a change
    /* 0x4BC */ byte       pad_4BC[0x4];
} ActorsShared801324fcWork;
STATIC_ASSERT_SIZEOF(ActorsShared801324fcWork, 0x4C0);

void ActorsShared801324fc(Task* task);

#endif
