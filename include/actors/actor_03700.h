#ifndef INCLUDE_ACTORS_ACTOR_03700_H
#define INCLUDE_ACTORS_ACTOR_03700_H

#include "main/task_types.h"

/// Bat TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR`, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor03700BatTaskDesc;

/// Slot export of `gActor03700BatTaskDesc` from actor_103700, with the same descriptor contract.
extern TaskDesc gActor103700BatTaskDesc;

/// Slot export of `gActor03700BatTaskDesc` from actor_203700, with the same descriptor contract.
extern TaskDesc gActor203700BatTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_03700_H
