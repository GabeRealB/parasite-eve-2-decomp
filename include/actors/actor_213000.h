#ifndef INCLUDE_ACTORS_ACTOR_213000_H
#define INCLUDE_ACTORS_ACTOR_213000_H

#include "main/task_types.h"

/// Eric Baldwin spawn recipes: 0 body, 1/2 held models, 3 three-root attachment, 4 right hand.
///
/// Priority 192. Indices 0..4 have no terminator.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Children borrow the body task
/// in `Task::spawnArg2`. Held models use part 8 and the right hand part 12 from
/// `Task::spawnArg1`; entry 3 attaches its three roots to body parts 9..11.
/// All children join the body teardown tree and borrow its lighting.
/// Keep actor_213000 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor213000EricBaldwinTaskDescriptors[5];

#endif // INCLUDE_ACTORS_ACTOR_213000_H
