#ifndef ACTORS_SHARED_80133580_H
#define ACTORS_SHARED_80133580_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::work`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks
/// of different sizes, so only the prefix this body reaches is described.
///
/// `yaw` is the same cache the neighbouring placement helpers write: the
/// heading the last opcode applied to the actor's root coordinate. `travel` is
/// the distance still to cover and is what makes this the "walk to" form of
/// that opcode rather than the "teleport to" form (`ActorsShared80132614`).
/// This body scales that distance by 30, where the sibling `ActorsShared801326ac`
/// scales it by 12 over the identical block.
typedef struct ActorsShared80133580Work {
    /* 0x000 */ byte pad_0[0x4E6];
    /* 0x4E6 */ u16  yaw;
    /* 0x4E8 */ byte pad_4E8[0x2];
    /* 0x4EA */ s16  travel;
} ActorsShared80133580Work;
STATIC_ASSERT_SIZEOF(ActorsShared80133580Work, 0x4EC);

/// Argument block of the script opcode this body implements: the world
/// position to walk to. Only the horizontal components are read.
typedef struct ActorsShared80133580Target {
    /* 0x00 */ VECTOR pos;
} ActorsShared80133580Target;
STATIC_ASSERT_SIZEOF(ActorsShared80133580Target, 0x10);

/// Aims the actor's root coordinate at `target`: takes the yaw of the
/// horizontal offset from the coordinate's own translation with `ratan2`,
/// caches it and rebuilds the local matrix from it, then records the remaining
/// distance for the walk that follows.
s32 ActorsShared80133580(Task* task, s32 arg1, ActorsShared80133580Target* target);

#endif
