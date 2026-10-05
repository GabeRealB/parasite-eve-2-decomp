#ifndef GAMEPLAY_PLANAR_REFLECTION_H
#define GAMEPLAY_PLANAR_REFLECTION_H

#include "main/task_types.h"

/// Dispatches a player-reflection task to the callback of its captured room.
///
/// Starts bodyless in state 0 with a live player and the matching room overlay
/// loaded. The room callback initializes the reflection and advances to state
/// 1; later ticks update it. The stage/area selector is shared and captured only
/// in state 0, so dispatch does not follow subsequent location changes. All
/// tasks using this dispatcher share the selector; initializing another task
/// replaces it. The selected overlay must remain loaded until the task is
/// released. Unsupported locations kill the task. `spawnArg1.value` selects
/// floor (0) or plane (1).
void planarReflectionDispatchPlayerTask(Task* reflectionTask);

#endif // GAMEPLAY_PLANAR_REFLECTION_H
