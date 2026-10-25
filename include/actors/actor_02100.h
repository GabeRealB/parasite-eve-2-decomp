#ifndef INCLUDE_ACTORS_ACTOR_02100_H
#define INCLUDE_ACTORS_ACTOR_02100_H

#include "main/task_types.h"

/// Watcher TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor02100WatcherTaskDesc;

/// Slot export of `gActor02100WatcherTaskDesc` from actor_102100, with the same descriptor contract.
extern TaskDesc gActor102100WatcherTaskDesc;

/// Slot export of `gActor02100WatcherTaskDesc` from actor_202100, with the same descriptor contract.
extern TaskDesc gActor202100WatcherTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_02100_H
