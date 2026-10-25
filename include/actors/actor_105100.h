#ifndef INCLUDE_ACTORS_ACTOR_105100_H
#define INCLUDE_ACTORS_ACTOR_105100_H

#include "main/task_types.h"

/// Stinger spawn recipes: 0 enemy TMD body, 1 fireball coordinate body, 2 beam coordinate body.
///
/// All execute at priority `TASK_PRIORITY_ACTOR`; indices 0..2 have no terminator. Enemy spawns
/// supply a live Enemy in spawn argument 2. Keep this package loaded until
/// the body and its coordinate children are released.
extern TaskDesc gActor105100TaskDescriptors[3];

#endif // INCLUDE_ACTORS_ACTOR_105100_H
