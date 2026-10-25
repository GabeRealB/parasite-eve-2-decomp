#ifndef INCLUDE_ACTORS_ACTOR_05700_H
#define INCLUDE_ACTORS_ACTOR_05700_H

#include "main/task_types.h"

/// Launcher Rook GOLEM spawn descriptors: 0 body, 1 launcher, 2 grenade, 3 shield.
///
/// All four recipes create TMD bodies at priority `TASK_PRIORITY_ACTOR` with automatic model
/// buffers. Indices address live entries; there is no terminator. Room
/// placements spawn entry 0, whose work borrows this table for its children.
/// Keep the actor package loaded while any spawned task, including a detached
/// grenade, uses its callbacks or borrowed model geometry.
extern TaskDesc gActor05700RookGolemTasks[4];

#endif // INCLUDE_ACTORS_ACTOR_05700_H
