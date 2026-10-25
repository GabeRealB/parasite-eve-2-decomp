#ifndef INCLUDE_ACTORS_ACTOR_356100_H
#define INCLUDE_ACTORS_ACTOR_356100_H

#include "main/task_types.h"

/// Horned Stranger spawn recipe for Neo Ark's forest zone.
///
/// TMD body at priority `TASK_PRIORITY_ACTOR` with automatic model buffers. Spawn at index 0 with
/// the owning Enemy in spawnArg2. Keep actor_356100 callbacks and borrowed
/// model/animation resources loaded through enemy teardown.
extern TaskDesc gActor356100HornedStrangerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_356100_H
