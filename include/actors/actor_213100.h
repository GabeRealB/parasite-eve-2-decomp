#ifndef INCLUDE_ACTORS_ACTOR_213100_H
#define INCLUDE_ACTORS_ACTOR_213100_H

#include "main/task_types.h"

/// Jodie Bouquet spawn recipes: 0 body, 1 held model.
///
/// Priority 192. Indices 0..1 have no terminator.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. The held model borrows the body
/// in `Task::spawnArg2` and part 8 in `Task::spawnArg1`, then joins its teardown tree.
/// Body setup tears down the actor if its held-model spawn fails.
/// Keep actor_213100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor213100JodieBouquetTaskDescriptors[2];

#endif // INCLUDE_ACTORS_ACTOR_213100_H
