#ifndef INCLUDE_ACTORS_ACTOR_323000_H
#define INCLUDE_ACTORS_ACTOR_323000_H

#include "main/task_types.h"

/// Desert Chaser spawn recipe for the main-street scripted encounter.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`; this package carries the
/// cutscene build of the Desert Chaser lifecycle.
/// Keep actor_323000 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor323000DesertChaserTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_323000_H
