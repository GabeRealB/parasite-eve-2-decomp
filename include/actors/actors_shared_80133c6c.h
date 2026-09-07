#ifndef ACTORS_SHARED_80133C6C_H
#define ACTORS_SHARED_80133C6C_H

#include "common.h"

#include "actors/actors_shared_8013411c.h"
#include "main/task.h"

/// Same placement record `ActorsShared8013411c` consumes: three longs of
/// translation followed by the Y/X/Z Euler angles.
typedef ActorShared8013411cPlacement ActorShared80133c6cPlacement;

void ActorsShared80133c6c(Task* task, s32 arg1, ActorShared80133c6cPlacement* placement);

#endif
