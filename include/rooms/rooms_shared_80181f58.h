#ifndef ROOMS_SHARED_80181F58_H
#define ROOMS_SHARED_80181F58_H

#include "common.h"

#include "main/task.h"

/// One tick of the rooms' rising-spark task: it walks the effect's angle on by
/// a random 0x200..0x3FF, rebuilds the velocity as a 3/16-scaled unit circle in
/// X and Z with a downward Y that grows with age, and spawns the child effect
/// at the task's own coordinate frame. The task releases its work block after
/// 0x15 ticks, or as soon as the room's fade reaches 4. Fifteen rooms carry
/// this body.
void RoomsShared80181f58(Task* task);

#endif // ROOMS_SHARED_80181F58_H
