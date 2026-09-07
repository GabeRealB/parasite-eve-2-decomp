#ifndef ACTORS_SHARED_80132774_H
#define ACTORS_SHARED_80132774_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc` and stores it into that field; the carriers
/// ask for different sizes (`actor_150400` for 0x4C0, `actor_451100` for
/// 0x4B8), so only the prefix this body reaches is described here rather than a
/// whole-block size that would be wrong for some of them.
///
/// `yaw` caches the heading the placement opcode last applied to the actor's
/// root coordinate.
typedef struct ActorsShared80132774Work {
    /* 0x000 */ byte pad_0[0x4AE];
    /* 0x4AE */ u16  yaw;
} ActorsShared80132774Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132774Work, 0x4B0);

/// Argument block of the script opcode this body implements: a world
/// translation followed by Euler angles, of which only the yaw (`rot.vy`) is
/// used.
typedef struct ActorsShared80132774Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} ActorsShared80132774Placement;
STATIC_ASSERT_SIZEOF(ActorsShared80132774Placement, 0x18);

/// Yaws the actor's root coordinate to `placement->rot.vy` and drops the
/// placement translation into that matrix.
s32 ActorsShared80132774(Task* task, s32 arg1, ActorsShared80132774Placement* placement);

#endif
