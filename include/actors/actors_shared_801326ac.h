#ifndef ACTORS_SHARED_801326AC_H
#define ACTORS_SHARED_801326AC_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks
/// of different sizes, so only the prefix this body reaches is described.
///
/// `yaw` is the same cache `ActorsShared80132614` writes: the heading the last
/// placement opcode applied to the actor's root coordinate. `travel` is the
/// distance still to cover, in units of 12, and is what makes this the
/// "walk to" form of that opcode rather than the "teleport to" form.
typedef struct ActorsShared801326acWork {
    /* 0x000 */ byte pad_0[0x4E6];
    /* 0x4E6 */ u16  yaw;
    /* 0x4E8 */ byte pad_4E8[0x2];
    /* 0x4EA */ s16  travel;
} ActorsShared801326acWork;
STATIC_ASSERT_SIZEOF(ActorsShared801326acWork, 0x4EC);

/// Argument block of the script opcode this body implements: the world
/// position to walk to. Only the horizontal components are read.
typedef struct ActorsShared801326acTarget {
    /* 0x00 */ VECTOR pos;
} ActorsShared801326acTarget;
STATIC_ASSERT_SIZEOF(ActorsShared801326acTarget, 0x10);

/// Aims the actor's root coordinate at `target`: takes the yaw of the
/// horizontal offset from the coordinate's own translation with `ratan2`,
/// caches it and rebuilds the local matrix from it, then records the remaining
/// distance in twelfths for the walk that follows.
s32 ActorsShared801326ac(Task* task, s32 arg1, ActorsShared801326acTarget* target);

#endif
