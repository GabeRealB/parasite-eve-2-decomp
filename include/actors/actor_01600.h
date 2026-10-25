#ifndef INCLUDE_ACTORS_ACTOR_01600_H
#define INCLUDE_ACTORS_ACTOR_01600_H

#include "main/task_types.h"

/// Slot export of `gActor01600ScavengerTaskDesc` from actor_101600, with the same descriptor contract.
extern TaskDesc gActor101600ScavengerTaskDesc;

/// Scavenger TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR`, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor01600ScavengerTaskDesc;

/// Slot export of `gActor01600ScavengerTaskDesc` from actor_301600, with the same descriptor contract.
extern TaskDesc gActor301600ScavengerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_01600_H
