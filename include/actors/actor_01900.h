#ifndef INCLUDE_ACTORS_ACTOR_01900_H
#define INCLUDE_ACTORS_ACTOR_01900_H

#include "main/task_types.h"

/// Grinning Stranger TMD spawn descriptor shared by actor_101900 and actor_301900.
///
/// Spawn at index 0 with a live owning `Enemy` in `Task::spawnArg2.pointer`.
/// Priority `TASK_PRIORITY_ACTOR` attaches the nineteen-part body and automatic model buffers.
/// Placement entry 19 selects it; keep the package callbacks and borrowed
/// model geometry loaded through enemy teardown.
extern TaskDesc gActor01900GrinningStrangerTaskDesc;

/// Slot-1 export of `gActor01900GrinningStrangerTaskDesc` from actor_101900.
extern TaskDesc gActor101900GrinningStrangerTaskDesc;

/// Slot-3 export of `gActor01900GrinningStrangerTaskDesc` from actor_301900.
extern TaskDesc gActor301900GrinningStrangerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_01900_H
