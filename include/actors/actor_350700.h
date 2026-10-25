#ifndef INCLUDE_ACTORS_ACTOR_350700_H
#define INCLUDE_ACTORS_ACTOR_350700_H

#include "main/task_types.h"

/// Masked Eve Brea reversing-walker spawn recipe for the B6 training room.
///
/// Nineteen-part TMD body at priority `TASK_PRIORITY_CONTROL`, with automatic model buffers disabled.
/// Spawn at index 0 with the owning Enemy in spawnArg2. Keep actor_350700 and
/// its borrowed model/animation resources loaded until the task is destroyed.
extern TaskDesc gActor350700MaskedEveReverseWalkTaskDesc;

/// Kyle Madigan spawn recipes: 0 walker body, 1 left hand, 2 right hand, 3 held gun.
///
/// Priority 192. Indices 0..3 have no terminator.
/// The body skips automatic primitive buffers; attached TMD entries use them.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Children borrow the body task
/// in `Task::spawnArg2` and part 8 (left/gun) or 12 (right) in `Task::spawnArg1`.
/// They join the body teardown tree and borrow its coordinates and lighting.
/// Keep actor_350700 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor350700KyleMadiganTaskDescriptors[4];

#endif // INCLUDE_ACTORS_ACTOR_350700_H
