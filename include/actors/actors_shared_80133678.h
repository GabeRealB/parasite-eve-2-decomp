#ifndef ACTORS_SHARED_80133678_H
#define ACTORS_SHARED_80133678_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::work`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc` and stores it into that field; the carriers
/// ask for different sizes (`actor_450800` for 0x504), so only the prefix this
/// body reaches is described here rather than a whole-block size that would be
/// wrong for some of them.
///
/// `yaw` is the same cache the neighbouring placement helpers write: the
/// heading the last opcode applied to the actor's root coordinate. `travel` is
/// the distance still to cover, in twelfths, and is what makes this the
/// "walk to" form of that opcode rather than the "teleport to" form. This body
/// keeps that cache at 0x4AE, where `ActorsShared801326ac` / `80133580` keep
/// theirs at 0x4E6.
typedef struct ActorsShared80133678Work {
    /* 0x000 */ byte pad_0[0x4AE];
    /* 0x4AE */ u16  yaw;
    /* 0x4B0 */ byte pad_4B0[0x2];
    /* 0x4B2 */ s16  travel;
} ActorsShared80133678Work;
STATIC_ASSERT_SIZEOF(ActorsShared80133678Work, 0x4B4);

/// Argument block of the script opcode this body implements: the world
/// position to walk to. Only the horizontal components are read.
typedef struct ActorsShared80133678Target {
    /* 0x00 */ VECTOR pos;
} ActorsShared80133678Target;
STATIC_ASSERT_SIZEOF(ActorsShared80133678Target, 0x10);

/// Aims the actor's root coordinate at `target`: takes the yaw of the
/// horizontal offset from the coordinate's own translation with `ratan2`,
/// caches it and rebuilds the local matrix from it, then records the remaining
/// distance in twelfths for the walk that follows.
s32 ActorsShared80133678(Task* task, s32 arg1, ActorsShared80133678Target* target);

#endif
