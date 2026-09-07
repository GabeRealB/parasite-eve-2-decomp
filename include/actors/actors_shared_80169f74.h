#ifndef ACTORS_SHARED_80169F74_H
#define ACTORS_SHARED_80169F74_H

#include "common.h"

#include "actors/actors_shared_8013411c.h"
#include "main/task.h"

/// Same placement record `ActorsShared8013411c` consumes: three longs of
/// translation followed by the X/Y/Z Euler angles.
typedef ActorShared8013411cPlacement ActorShared80169f74Placement;

/// Work block hanging off `Task::idMap`. Only the prefix this body reaches is
/// described: `yaw` is the heading taken from the root coordinate's Z-axis
/// after the placement rotations are applied.
typedef struct ActorsShared80169f74Work {
    /* 0x00 */ byte pad_0[0x16];
    /* 0x16 */ s16  yaw;
} ActorsShared80169f74Work;

s32 ActorsShared80169f74(Task* task, s32 arg1, ActorShared80169f74Placement* placement);

#endif
