#ifndef ACTORS_SHARED_80132FE8_H
#define ACTORS_SHARED_80132FE8_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks
/// of different sizes, so only the prefix this body reaches is described.
///
/// `yaw` is the same cache the neighbouring placement helpers write: the
/// heading the last opcode applied to the actor's root coordinate. `travel` is
/// the distance still to cover and is what makes this the "walk to" form of
/// that opcode rather than the "teleport to" form (`ActorsShared80132614`).
/// This body scales that distance by 17, where the sibling `ActorsShared801326ac`
/// scales it by 12 and `ActorsShared80133580` by 30 over their own blocks.
typedef struct ActorsShared80132fe8Work {
    /* 0x000 */ byte pad_0[0x4AE];
    /* 0x4AE */ u16  yaw;
    /* 0x4B0 */ byte pad_4B0[0x2];
    /* 0x4B2 */ s16  travel;
} ActorsShared80132fe8Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132fe8Work, 0x4B4);

/// Argument block of the script opcode this body implements: the world
/// position to walk to. Only the horizontal components are read.
typedef struct ActorsShared80132fe8Target {
    /* 0x00 */ VECTOR pos;
} ActorsShared80132fe8Target;
STATIC_ASSERT_SIZEOF(ActorsShared80132fe8Target, 0x10);

/// Aims the actor's root coordinate at `target`: takes the yaw of the
/// horizontal offset from the coordinate's own translation with `ratan2`,
/// caches it and rebuilds the local matrix from it, then records the remaining
/// distance for the walk that follows.
s32 ActorsShared80132fe8(Task* task, s32 arg1, ActorsShared80132fe8Target* target);

#endif
