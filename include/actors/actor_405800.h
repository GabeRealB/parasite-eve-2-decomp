#ifndef INCLUDE_ACTORS_ACTOR_405800_H
#define INCLUDE_ACTORS_ACTOR_405800_H

#include "main/task_types.h"

/// Ivory Stalker body spawn recipe; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0 with the owning Enemy in spawnArg2. The body initializes
/// its attached arms; keep actor_405800 callbacks and borrowed geometry loaded.
extern TaskDesc gActor405800IvoryStalkerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_405800_H
