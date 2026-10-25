#ifndef INCLUDE_ACTORS_ACTOR_141000_H
#define INCLUDE_ACTORS_ACTOR_141000_H

#include "main/task_types.h"

/// Entry-scene flight recipes: 0 TMD controller, 1 coordinate beam, 2 smoke emitter.
///
/// All run at priority 192. Spawn entry 0 with unused arguments to unfold and fly
/// the model. Entry 1 borrows the controller task through spawn argument 2 and
/// joins its teardown tree; entry 2 takes a copied parent-frame translation and
/// emits smoke independently. No terminator. Keep actor_141000 and its resources
/// loaded until every task using these callbacks and model data has ended.
extern TaskDesc gActor141000FlightTasks[3];

/// Scripted nineteen-part Eve walker with manual model-buffer recovery.
///
/// Creates a TMD body at priority `TASK_PRIORITY_CONTROL`, initially skipping automatic primitive
/// buffer allocation. Area placement supplies the task-owned `Enemy` in spawn
/// argument 2. Initialization installs animation, placement, visibility, walk,
/// pace and facial-texture messages. Keep actor_141000 loaded until teardown.
extern TaskDesc gActor141000EveWalkerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_141000_H
