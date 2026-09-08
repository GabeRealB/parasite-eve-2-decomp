#ifndef ACTORS_SHARED_80164904_H
#define ACTORS_SHARED_80164904_H

#include "common.h"

#include "main/task.h"

/// Shared "should this actor keep acting" predicate. Returns 1 while the
/// enemy still has HP; once it is down, the actor's model flags decide -- bit
/// 0x80 (the model is being torn down) and bit 2 both answer 0, anything else
/// answers 1.
///
/// `Task::spawnArg2` is the `GpEnemy` the actor was spawned for and
/// `Task::extra` its `TmdObject`.
s32 ActorsShared80164904(Task* task);

#endif
