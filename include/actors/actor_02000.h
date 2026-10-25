#ifndef INCLUDE_ACTORS_ACTOR_02000_H
#define INCLUDE_ACTORS_ACTOR_02000_H

#include "main/task_types.h"

/// Slot export of `gActor02000PawnGolemTasks` from actor_102000, with the same descriptor contract.
extern TaskDesc gActor102000PawnGolemTasks[2];

/// Beam Sword Pawn GOLEM spawn descriptors: 0 body, 1 attached sword.
///
/// Both are priority-96 TMD tasks with automatic model buffers. Indices 0..1
/// select descriptors without a terminator. Keep this actor package loaded
/// while the callbacks, model geometry and body animation bank are borrowed.
extern TaskDesc gActor02000PawnGolemTasks[2];

/// Slot export of `gActor02000PawnGolemTasks` from actor_202000, with the same descriptor contract.
extern TaskDesc gActor202000PawnGolemTasks[2];

/// Slot export of `gActor02000PawnGolemTasks` from actor_302000, with the same descriptor contract.
extern TaskDesc gActor302000PawnGolemTasks[2];

#endif // INCLUDE_ACTORS_ACTOR_02000_H
