#ifndef INCLUDE_ACTORS_ACTOR_350500_H
#define INCLUDE_ACTORS_ACTOR_350500_H

#include "main/task_types.h"

/// Masked Eve Brea reversing-walker spawn recipe for the B6 corridor.
///
/// Nineteen-part TMD body at priority `TASK_PRIORITY_CONTROL`, with automatic model buffers disabled.
/// Spawn at index 0 with the owning Enemy in spawnArg2. Keep actor_350500 and
/// its borrowed model/animation resources loaded until the task is destroyed.
extern TaskDesc gActor350500MaskedEveReverseWalkTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_350500_H
