#ifndef INCLUDE_ACTORS_ACTOR_800300_H
#define INCLUDE_ACTORS_ACTOR_800300_H

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Models those descriptors attach.
extern TmdSource gActor800300Model02CF4;

/// Runs the noncombatant companion's initialization, frame update and teardown states.
///
/// The resident model-task descriptor supplies the model; spawning supplies
/// zeroed GameActor/CompanionWork storage and loaded native animation resources.
/// Task state must be 0..3. Initialization publishes the companion task and
/// borrows its work/model coordinates for collision until teardown unlinks them.
void actor800300Task(Task* task);

#endif // INCLUDE_ACTORS_ACTOR_800300_H
