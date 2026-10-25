#ifndef INCLUDE_ACTORS_ACTOR_120400_H
#define INCLUDE_ACTORS_ACTOR_120400_H

#include "main/task_types.h"

/// Kyle Madigan spawn recipes: 0 walker body, 1 left hand, 2 right hand.
///
/// Priority 192. Indices 0..2 have no terminator.
/// The body skips automatic primitive buffers; attached TMD entries use them.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Hands borrow the body task
/// in `Task::spawnArg2` and part 8 or 12 in `Task::spawnArg1`, then join its teardown tree.
/// Keep actor_120400 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor120400KyleMadiganTaskDescriptors[3];

#endif // INCLUDE_ACTORS_ACTOR_120400_H
