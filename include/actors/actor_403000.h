#ifndef INCLUDE_ACTORS_ACTOR_403000_H
#define INCLUDE_ACTORS_ACTOR_403000_H

#include "main/task_types.h"

/// Blizzard Chaser spawn recipe for the Mine Cavern encounter.
///
/// Spawn at index 0 with priority 96. The 24-part TMD body skips automatic
/// primitive-buffer allocation. `Task::spawnArg2` borrows a live `Enemy` for
/// setup, update and teardown. Keep actor_403000 code and its borrowed model
/// and animation resources loaded while the spawned actor uses them.
extern TaskDesc gActor403000TaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_403000_H
