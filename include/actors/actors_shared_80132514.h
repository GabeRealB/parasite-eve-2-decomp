#ifndef ACTORS_SHARED_80132514_H
#define ACTORS_SHARED_80132514_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks
/// of different sizes, so only the prefix this body reaches is described.
///
/// `anim` is the animation context at +0x40; twenty 0x28-byte `GpAnimSlot`s
/// follow it and this body walks slots 1..0x13, setting `field_9` then calling
/// `Gp_AnimResetSlot`. `field_4B8` is the animation id those slots are seeded
/// with, and `field_4B6` is the copy kept for change detection (same pair
/// `ActorsShared801324c8` uses).
typedef struct ActorsShared80132514Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x142];
    /* 0x4B6 */ s16        field_4B6;
    /* 0x4B8 */ s16        field_4B8;
    /* 0x4BA */ byte       pad_4BA[2];
} ActorsShared80132514Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132514Work, 0x4BC);

/// Resets animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void ActorsShared80132514(Task* task);

#endif
