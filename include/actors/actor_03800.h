#ifndef INCLUDE_ACTORS_ACTOR_03800_H
#define INCLUDE_ACTORS_ACTOR_03800_H

#include "main/task_types.h"

/// Black Beetle TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor03800BlackBeetleTaskDesc;

/// Slot export of `gActor03800BlackBeetleTaskDesc` from actor_103800, with the same descriptor contract.
extern TaskDesc gActor103800BlackBeetleTaskDesc;

/// Slot export of `gActor03800BlackBeetleTaskDesc` from actor_203800, with the same descriptor contract.
extern TaskDesc gActor203800BlackBeetleTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_03800_H
