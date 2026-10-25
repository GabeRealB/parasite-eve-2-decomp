#ifndef INCLUDE_ACTORS_ACTOR_04600_H
#define INCLUDE_ACTORS_ACTOR_04600_H

#include "main/task_types.h"

/// Standing Sucklerceph TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor04600SucklercephTaskDesc;

/// Skull Stalker TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor04600SkullStalkerTaskDesc;

/// Slot export of `gActor04600SucklercephTaskDesc` from actor_104600, with the same descriptor contract.
extern TaskDesc gActor104600SucklercephTaskDesc;

/// Slot export of `gActor04600SkullStalkerTaskDesc` from actor_104600, with the same descriptor contract.
extern TaskDesc gActor104600SkullStalkerTaskDesc;

/// Slot export of `gActor04600SucklercephTaskDesc` from actor_204600, with the same descriptor contract.
extern TaskDesc gActor204600SucklercephTaskDesc;

/// Slot export of `gActor04600SkullStalkerTaskDesc` from actor_204600, with the same descriptor contract.
extern TaskDesc gActor204600SkullStalkerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_04600_H
