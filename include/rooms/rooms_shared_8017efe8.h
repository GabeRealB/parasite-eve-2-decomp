#ifndef ROOMS_SHARED_8017EFE8_H
#define ROOMS_SHARED_8017EFE8_H

#include "common.h"

#include "main/task.h"

/// Per-frame update of an effect task drawn with `Room_Draw19` (state 1) or
/// `Room_Draw23` (state 2). State 0 seeds the work from `spawnArg1`: the two
/// draw parameters (the second one random), the frame period and, when the
/// spawner left `field_10` zero, a velocity whose shape is picked by bits 24-27
/// and which is scaled to `field_2A` through the GTE. Later ticks draw, drift
/// the coordinate by that velocity with `vy` growing by 6 each tick, and
/// advance the frame every `field_28` ticks, releasing the task after frame 7.
/// While an event is running the task only draws, and it is released once the
/// event state reaches 4.
void RoomsShared8017efe8(Task* task);

#endif // ROOMS_SHARED_8017EFE8_H
