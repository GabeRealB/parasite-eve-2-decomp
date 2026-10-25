#ifndef INCLUDE_ACTORS_ACTOR_403100_H
#define INCLUDE_ACTORS_ACTOR_403100_H

#include "main/task_types.h"

/// Spawn recipes for the Burner model (index 0) and its floor-puff emitter (index 1).
///
/// Both use priority 96. The model recipe owns a TMD task and defers primitive
/// buffer allocation; spawn argument 2 borrows the live Enemy. The emitter
/// owns a single coordinate body. Keep actor_403100 and its model assets loaded
/// throughout either task's lifetime.
extern TaskDesc gActor403100TaskDescriptors[2];

#endif // INCLUDE_ACTORS_ACTOR_403100_H
