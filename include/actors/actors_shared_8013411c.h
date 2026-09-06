#ifndef ACTORS_SHARED_8013411C_H
#define ACTORS_SHARED_8013411C_H

#include "common.h"
#include <psyq/libgte.h>

#include "main/task.h"

/// Position + Euler rotation handed to `ActorsShared8013411c`, which copies it
/// onto a task's `TmdObject` coordinate frame (`Task::extra->field_8`): the
/// three longs become the coordinate's translation and the three shorts are
/// the Euler angles, applied Y then X then Z via `Gfx_RotMatrix*`. Actor
/// overlays keep one of these per placed object in their own `.data`.
typedef struct ActorShared8013411cPlacement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} ActorShared8013411cPlacement;

void ActorsShared8013411c(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);

#endif
