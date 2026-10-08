#ifndef INCLUDE_ACTORS_ACTOR_800200_H
#define INCLUDE_ACTORS_ACTOR_800200_H

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gActor800200FlintBody;

/// Dispatches Flint's setup, active update, exit staging or teardown task state.
///
/// Requires Task::state in 0..3 and the loaded actor_800200 overlay. State 0
/// initializes the model/work used by later states; state 3 kills the task.
/// The resident bank-7 descriptor supplies the live TMD task and body resource.
void actor800200Task(Task* task);

#endif // INCLUDE_ACTORS_ACTOR_800200_H
