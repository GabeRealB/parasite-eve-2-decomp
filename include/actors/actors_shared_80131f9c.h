#ifndef ACTORS_SHARED_80131F9C_H
#define ACTORS_SHARED_80131F9C_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The `ActorsShared80131e24` two-state dispatcher with one extra store: it
/// copies `Task::idMap` into an overlay-local global before calling the
/// handler, which is how the rest of each carrier reaches its work block
/// without the task in hand.
///
/// The block behind that global is per overlay, so the shared unit declares it
/// `void*`; a carrier that reads it from C declares the same symbol again with
/// its own work type (see `include/actors/actor_143900.h`).
extern void* ActorsShared80131f9cWork;

void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task);
void ActorsShared80131f9cSub1(GpEnemy* enemy, Task* task);

void ActorsShared80131f9c(Task* task);

#endif
