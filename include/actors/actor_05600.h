#ifndef INCLUDE_ACTORS_ACTOR_05600_H
#define INCLUDE_ACTORS_ACTOR_05600_H

#include "main/task_types.h"

/// Slot export of `gActor05600PawnGolemTasks` from actor_105600, with the same descriptor contract.
extern TaskDesc gActor105600PawnGolemTasks[3];

/// Launcher Pawn GOLEM spawn descriptors: 0 body, 1 launcher, 2 grenade; all are TMD bodies.
///
/// Indices select live descriptors without a terminator. Keep this package
/// loaded while its callbacks and borrowed model geometry are in use.
extern TaskDesc gActor05600PawnGolemTasks[3];

/// Slot export of `gActor05600PawnGolemTasks` from actor_205600, with the same descriptor contract.
extern TaskDesc gActor205600PawnGolemTasks[3];

#endif // INCLUDE_ACTORS_ACTOR_05600_H
