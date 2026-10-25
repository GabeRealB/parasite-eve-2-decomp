#ifndef INCLUDE_ACTORS_ACTOR_160900_H
#define INCLUDE_ACTORS_ACTOR_160900_H

#include "main/task_types.h"

/// Aya/Kyle cutscene spawn recipes used by the pod service gantry.
///
/// Eight live entries, without a terminator, all at priority `TASK_PRIORITY_CONTROL`: 0 cutscene,
/// 1 reveal fade, 2 darkening fade, 3 Kyle body, 4 handgun, 5 gun hand,
/// 6 free hand, 7 light quad. Entry 0 takes unused spawn arguments. Fades use
/// argument 1's low unsigned halfword as intensity units per update. Models
/// skip automatic primitive buffers: the body requires the published cutscene;
/// attachments borrow the body in argument 2 and select free hand (0), gun
/// hand (1) or gun (2) in argument 1. Quads use a coordinate body and a white-
/// corner pattern in argument 1 (0..5); their spawner supplies corner work.
/// Keep actor_160900, its models, animation data and CAP/scene resources loaded
/// through script completion and teardown of every spawned task.
extern TaskDesc gActor160900TaskDescriptors[8];

#endif // INCLUDE_ACTORS_ACTOR_160900_H
