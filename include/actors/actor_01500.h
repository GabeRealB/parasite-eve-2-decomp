#ifndef INCLUDE_ACTORS_ACTOR_01500_H
#define INCLUDE_ACTORS_ACTOR_01500_H

#include "main/task_types.h"

/// Mind Suckler TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor01500MindSucklerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_01500_H
