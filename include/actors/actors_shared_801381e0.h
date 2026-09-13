#ifndef ACTORS_SHARED_801381E0_H
#define ACTORS_SHARED_801381E0_H

#include "common.h"

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here -- the same block
/// `Actor402200Work` describes, seen through the one field this body reaches.
/// `field_6F4` is the phase halfword the carriers' frame handlers branch on.
typedef struct ActorsShared801381e0Work {
    /* 0x000 */ byte pad_0[0x6F4];
    /* 0x6F4 */ s16  field_6F4;
} ActorsShared801381e0Work;

/// Raises the actor's phase to 1 while the remaining-enemy count is positive,
/// which is what keeps the carrier's frame handler in its combat states; the
/// actor returns 0 either way.
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
s32 ActorsShared801381e0(Task* task);

#endif
