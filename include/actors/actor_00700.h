#ifndef INCLUDE_ACTORS_ACTOR_00700_H
#define INCLUDE_ACTORS_ACTOR_00700_H

#include "main/task_types.h"

/// Rat TMD spawn descriptor shared by actor_100700 and actor_200700; priority 96.
///
/// Spawn at index 0 with a live `Enemy` in `Task::spawnArg2.pointer`.
/// Automatic model buffers attach the seven-part rat; keep its package loaded
/// until enemy teardown. Placement entry 7 selects this descriptor.
extern TaskDesc gActor00700RatTaskDesc;

/// Moth TMD spawn descriptor shared by actor_100700 and actor_200700; priority 96.
///
/// Spawn at index 0 with a live `Enemy` in `Task::spawnArg2.pointer`.
/// Automatic model buffers attach the four-part moth; keep its package loaded
/// until enemy teardown. Placement entry 8 selects this descriptor.
extern TaskDesc gActor00700MothTaskDesc;

/// Slot-2 export of `gActor00700RatTaskDesc`, with the same spawn contract.
extern TaskDesc gActor200700RatTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_00700_H
