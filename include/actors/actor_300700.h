#ifndef INCLUDE_ACTORS_ACTOR_300700_H
#define INCLUDE_ACTORS_ACTOR_300700_H

#include "main/task_types.h"

/// Moth TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor300700MothTaskDesc;

/// Rat TMD spawn descriptor exported by actor_300700; priority 96.
///
/// Spawn at index 0 with a live `Enemy` in `Task::spawnArg2.pointer`.
/// Automatic model buffers attach the seven-part rat; placement entry 7
/// selects it through file-group base 30. Keep the package callbacks and
/// borrowed geometry loaded until enemy teardown.
extern TaskDesc gActor300700RatTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_300700_H
