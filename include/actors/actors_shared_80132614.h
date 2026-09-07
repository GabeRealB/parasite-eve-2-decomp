#ifndef ACTORS_SHARED_80132614_H
#define ACTORS_SHARED_80132614_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc` and stores it into that field; the carriers
/// ask for different sizes (`actor_160600` for 0x4F8, `actor_161500` for
/// 0x4FC, `actor_450800` for 0x504), so only the prefix this body reaches is
/// described here rather than a whole-block size that would be wrong for some
/// of them.
///
/// `yaw` caches the heading the placement opcode last applied to the actor's
/// root coordinate. This body is the sibling of `ActorsShared80132774`, which
/// is the same routine over a work block that keeps that cache at 0x4AE
/// instead.
typedef struct ActorsShared80132614Work {
    /* 0x000 */ byte pad_0[0x4E6];
    /* 0x4E6 */ u16  yaw;
} ActorsShared80132614Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132614Work, 0x4E8);

/// Argument block of the script opcode this body implements: a world
/// translation followed by Euler angles, of which only the yaw (`rot.vy`) is
/// used.
typedef struct ActorsShared80132614Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} ActorsShared80132614Placement;
STATIC_ASSERT_SIZEOF(ActorsShared80132614Placement, 0x18);

/// Yaws the actor's root coordinate to `placement->rot.vy` and drops the
/// placement translation into that matrix.
s32 ActorsShared80132614(Task* task, s32 arg1, ActorsShared80132614Placement* placement);

#endif
