#ifndef INCLUDE_ACTORS_ACTOR_401800_H
#define INCLUDE_ACTORS_ACTOR_401800_H

#include "main/task_types.h"

/// Odd Stranger TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR`, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor401800OddStrangerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_401800_H
