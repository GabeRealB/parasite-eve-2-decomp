#ifndef INCLUDE_ACTORS_ACTOR_207200_H
#define INCLUDE_ACTORS_ACTOR_207200_H

#include "main/task_types.h"

/// Skull Stalker TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor207200SkullStalkerTaskDesc;

/// Creeping Stranger TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor207200CreepingStrangerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_207200_H
