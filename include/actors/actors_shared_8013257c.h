#ifndef ACTORS_SHARED_8013257C_H
#define ACTORS_SHARED_8013257C_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc` and stores it into that field; the four
/// overlays ask for different sizes, so only the prefix this body reaches is
/// described here rather than a whole-block size that would be wrong for three
/// of them.
///
/// `anim` is the animation context `Gp_AnimTickIndex` and friends walk.
typedef struct ActorsShared8013257cWork {
    /* 0x00 */ byte      pad_0[0x40];
    /* 0x40 */ GpAnimCtx anim;
} ActorsShared8013257cWork;
STATIC_ASSERT_SIZEOF(ActorsShared8013257cWork, 0x54);

/// Ticks animation slots 1..0x12 of the actor's animation context.
void ActorsShared8013257c(Task* task);

#endif
