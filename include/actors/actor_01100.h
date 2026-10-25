#ifndef INCLUDE_ACTORS_ACTOR_01100_H
#define INCLUDE_ACTORS_ACTOR_01100_H

#include "main/task_types.h"

/// Mossback spawn descriptors: 0 body, 1 coordinate-body spit glob, 2 coordinate-body spit cloud.
///
/// Spawn indices are 0..2, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor01100Tasks[3];

/// Slot export of `gActor01100Tasks` from actor_101100, with the same descriptor contract.
extern TaskDesc gActor101100Tasks[3];

/// Slot export of `gActor01100Tasks` from actor_201100, with the same descriptor contract.
extern TaskDesc gActor201100Tasks[3];

/// Slot export of `gActor01100Tasks` from actor_301100, with the same descriptor contract.
extern TaskDesc gActor301100Tasks[3];

#endif // INCLUDE_ACTORS_ACTOR_01100_H
