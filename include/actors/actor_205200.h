#ifndef INCLUDE_ACTORS_ACTOR_205200_H
#define INCLUDE_ACTORS_ACTOR_205200_H

#include "main/task_types.h"

/// Coordinate-body recipes: 0 destructible-part controller, 1 child part.
///
/// Both execute at priority `TASK_PRIORITY_ACTOR`; the two live entries have no end marker. Enemy
/// spawns supply a live `Enemy` in spawn argument 2. Controller placement modes
/// 1..3 select the tunnel, corridor or training room. The parent controller and
/// its work must outlive each child task. Keep actor_205200 and room resources loaded.
extern TaskDesc gActor205200ControllerPartTasks[2];

/// Stationary masked Eve TMD recipe at priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index zero with a live `Enemy` in spawn argument 2. Placement mode
/// 0 selects the Shelter B6 corridor, 1 the training room. Keep actor_205200,
/// its borrowed model geometry and the selected room resources loaded while live.
extern TaskDesc gActor205200BodyTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_205200_H
