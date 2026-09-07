#ifndef ACTORS_SHARED_80132404_H
#define ACTORS_SHARED_80132404_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc` and stores it into that field; the carriers
/// ask for different sizes, so only the prefix this body reaches is described
/// here rather than a whole-block size that would be wrong for most of them.
///
/// `anim` is the animation context `Gp_AnimTickIndex` and friends walk.
typedef struct ActorsShared80132404Work {
    /* 0x00 */ byte      pad_0[0x40];
    /* 0x40 */ GpAnimCtx anim;
} ActorsShared80132404Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132404Work, 0x54);

/// Ticks animation slots 1..0x13 of the actor's animation context.
void ActorsShared80132404(Task* task);

#endif
