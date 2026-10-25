#ifndef INCLUDE_ACTORS_ACTOR_323300_H
#define INCLUDE_ACTORS_ACTOR_323300_H

#include "main/task_types.h"

/// Toilet encounter spawn recipes: 0 woman, 1 Lesser Stranger revealed by her command.
///
/// Priority 192. Indices 0..1 have no terminator.
/// The woman skips automatic primitive buffers; the Stranger uses them.
/// Body entry 0 borrows its live `Enemy` in `Task::spawnArg2`. Entry 1 receives
/// zero spawn arguments; its setup establishes its own placement and work.
/// Keep actor_323300 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor323300WomanStrangerTaskDescriptors[2];

#endif // INCLUDE_ACTORS_ACTOR_323300_H
