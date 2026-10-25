#ifndef INCLUDE_ACTORS_ACTOR_206100_H
#define INCLUDE_ACTORS_ACTOR_206100_H

#include "main/task_types.h"

/// Sea Diver spawn recipes: 0 enemy body, 1 flying shot with a coordinate body.
///
/// Priority 96. Indices 0..1 have no terminator.
/// The TMD body uses automatic primitive buffers; the shot has no TMD buffer.
/// Body `Task::spawnArg2` borrows its live `Enemy`. The shot spawner supplies its
/// work and world transform before its first dispatch; descriptor data is unused.
/// Keep actor_206100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor206100SeaDiverTaskDescriptors[2];

#endif // INCLUDE_ACTORS_ACTOR_206100_H
