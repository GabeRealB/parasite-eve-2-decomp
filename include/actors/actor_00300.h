#ifndef INCLUDE_ACTORS_ACTOR_00300_H
#define INCLUDE_ACTORS_ACTOR_00300_H

#include "main/task_types.h"

/// Brain Stinger spawn descriptors: 0 enemy body, 1 drain attachment, 2 flying fireball.
///
/// Entries 0 and 1 allocate TMD bodies with automatic buffers; entry 2 allocates
/// one coordinate body. All run at priority 96. Indices are 0..2, without a
/// terminator. Keep the callbacks and borrowed models loaded for the tasks' lives;
/// the drain child additionally borrows its parent enemy's work and lighting.
extern TaskDesc gActor00300Tasks[3];

/// Slot export of `gActor00300Tasks` from actor_100300, with the same descriptor contract.
extern TaskDesc gActor100300Tasks[3];

/// Slot export of `gActor00300Tasks` from actor_200300, with the same descriptor contract.
extern TaskDesc gActor200300Tasks[3];

#endif // INCLUDE_ACTORS_ACTOR_00300_H
