#ifndef INCLUDE_ACTORS_ACTOR_421600_H
#define INCLUDE_ACTORS_ACTOR_421600_H

#include "main/task_types.h"

/// Water Tower Desert Chaser spawn recipe; priority `TASK_PRIORITY_ACTOR`, automatic model buffers disabled.
///
/// Spawn at index 0 with the owning Enemy in spawnArg2. The TMD body, animation
/// resources and actor_421600 callbacks must remain loaded through enemy teardown.
extern TaskDesc gActor421600DesertChaserTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_421600_H
