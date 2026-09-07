#ifndef ACTORS_SHARED_80135990_H
#define ACTORS_SHARED_80135990_H

#include "common.h"

#include "actors/actors_shared_8013411c.h"
#include "main/task.h"

/// Same placement record `ActorsShared8013411c` consumes: three longs of
/// translation followed by the X/Y/Z Euler angles.
typedef ActorShared8013411cPlacement ActorShared80135990Placement;

s32 ActorsShared80135990(Task* task, s32 arg1, ActorShared80135990Placement* placement);

#endif
