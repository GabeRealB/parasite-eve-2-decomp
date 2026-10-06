#ifndef INCLUDE_ACTORS_ACTOR_800100_H
#define INCLUDE_ACTORS_ACTOR_800100_H

#include "main/task_types.h"

void func_actor_800100_80163CF0(Task* task);

void func_actor_800100_80161F20(Task* task);

/// Updates one flame emitted by the companion's Pyke attachment.
///
/// Requires a counted effect task with an owned coordinate body and `EffectWork`
/// in `spawnArg2.pointer`, initially age/state zero and work NULL. The low u16
/// of `spawnArg1.value` supplies launch speed in game units per running tick
/// (0x40..0x180) and initial billboard size before a 0x180 bias.
/// Launch follows the coordinate's positive Y axis; a geometry hit starts drift
/// and an enemy contact ends flight. Checks expiry after drawing at age 21 or
/// later, except on the tick entering drift.
/// Owns collision work after initialization and releases the effect on expiry
/// or cancellation. Paused updates refresh the composed coordinate and redraw.
void actor800100PykeFlameTask(Task* task);

#endif // INCLUDE_ACTORS_ACTOR_800100_H
