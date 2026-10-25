#ifndef INCLUDE_ACTORS_ACTOR_02500_H
#define INCLUDE_ACTORS_ACTOR_02500_H

#include "main/task_types.h"

/// Slot export of `gActor02500Tasks` from actor_102500, with the same descriptor contract.
extern TaskDesc gActor102500Tasks[2];

/// Scorpion spawn descriptors: 0 TMD body, 1 coordinate-body corpse poison.
///
/// Indices select live descriptors without a terminator. Keep this package
/// loaded while its callbacks and borrowed model geometry are in use.
extern TaskDesc gActor02500Tasks[2];

#endif // INCLUDE_ACTORS_ACTOR_02500_H
