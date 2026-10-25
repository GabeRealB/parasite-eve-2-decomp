#ifndef INCLUDE_ACTORS_ACTOR_402200_H
#define INCLUDE_ACTORS_ACTOR_402200_H

#include "main/task_types.h"

/// Knight GOLEM model-task descriptor exported to room resource tables.
///
/// Attaches the nineteen-part GOLEM body at task priority `TASK_PRIORITY_ACTOR`; the second spawn
/// argument borrows the live Enemy through spawn, living frames and corpse frames.
extern TaskDesc gActor402200GolemTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_402200_H
