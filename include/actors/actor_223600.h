#ifndef INCLUDE_ACTORS_ACTOR_223600_H
#define INCLUDE_ACTORS_ACTOR_223600_H

#include "main/task_types.h"

/// Blood Suckler spawn recipe for the general-store scripted figure.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy` for setup, update and teardown.
/// Keep actor_223600 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor223600BloodSucklerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_223600_H
