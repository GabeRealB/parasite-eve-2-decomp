#ifndef ACTORS_SHARED_80164954_H
#define ACTORS_SHARED_80164954_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Placement `ActorsShared80164954` copies onto the actor's root coordinate:
/// the three longs are the world translation and the three shorts are the
/// Euler angles, applied yaw / pitch / roll.
typedef struct ActorShared80164954Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} ActorShared80164954Placement;

s32 ActorsShared80164954(Task* task, s32 arg1, ActorShared80164954Placement* placement);

#endif
