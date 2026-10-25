#ifndef INCLUDE_ACTORS_ACTOR_135600_H
#define INCLUDE_ACTORS_ACTOR_135600_H

#include "main/task_types.h"

/// Kyle Madigan spawn recipes: 0 walker body, 1 left hand, 2 right hand, 3 held model.
///
/// Priority 192. Indices 0..3 have no terminator.
/// The body skips automatic primitive buffers; attached TMD entries use them.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Children borrow the body task
/// in `Task::spawnArg2` and part 8 (left/held) or 12 (right) in `Task::spawnArg1`.
/// They join the body teardown tree and borrow its coordinates and lighting.
/// Keep actor_135600 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor135600KyleMadiganTaskDescriptors[4];

#endif // INCLUDE_ACTORS_ACTOR_135600_H
