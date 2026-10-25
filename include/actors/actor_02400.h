#ifndef INCLUDE_ACTORS_ACTOR_02400_H
#define INCLUDE_ACTORS_ACTOR_02400_H

#include "main/task_types.h"

/// Amoeba spawn descriptors: 0 TMD body, 1 coordinate-body fireball.
///
/// Spawn indices are 0..1, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor02400Tasks[2];

/// Slot export of `gActor02400Tasks` from actor_102400, with the same descriptor contract.
extern TaskDesc gActor102400Tasks[2];

/// Slot export of `gActor02400Tasks` from actor_202400, with the same descriptor contract.
extern TaskDesc gActor202400Tasks[2];

#endif // INCLUDE_ACTORS_ACTOR_02400_H
