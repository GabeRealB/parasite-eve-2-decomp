#ifndef INCLUDE_ACTORS_ACTOR_02300_H
#define INCLUDE_ACTORS_ACTOR_02300_H

#include "main/task_types.h"

/// Beam Sword Rook GOLEM spawn descriptors: 0 body, 1 sword, 2 shield; all are TMD bodies.
///
/// Spawn indices are 0..2, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor02300RookGolemTasks[3];

/// Slot export of `gActor02300RookGolemTasks` from actor_102300, with the same descriptor contract.
extern TaskDesc gActor102300RookGolemTasks[3];

/// Slot export of `gActor02300RookGolemTasks` from actor_202300, with the same descriptor contract.
extern TaskDesc gActor202300RookGolemTasks[3];

#endif // INCLUDE_ACTORS_ACTOR_02300_H
