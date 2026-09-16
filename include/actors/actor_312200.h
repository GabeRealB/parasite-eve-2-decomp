#ifndef ACTOR_312200_H
#define ACTOR_312200_H

#include "common.h"

#include "actors/actors_shared_80169f74.h"
#include "main/task.h"

/// Private work block of the actor 312200 task, hanging off `Task::idMap`.
///
/// Only the fields the matched code touches are named so far: `yaw` at 0x08 is
/// the heading `func_actor_312200_801635CC` reads back from the root
/// coordinate, two halfwords earlier than `ActorsShared80169f74Work::yaw`. The
/// block is much larger - sibling `func_actor_312200_801636CC` writes halfwords
/// at 0x0, 0x88C, 0x892 and 0x8B4..0x8B8 of the same pointer - so the struct
/// stays open-ended.
typedef struct Actor312200Work {
    /* 0x00 */ byte pad_0[0x8];
    /* 0x08 */ s16  yaw;
} Actor312200Work;

/// Placement opcode: the three longs of `placement->pos` are copied onto the
/// actor's root coordinate, the Euler angles are applied X / Y / Z, and the
/// resulting heading is read back out of the matrix Z-axis with `ratan2` and
/// cached in the work block.
s32 func_actor_312200_801635CC(Task* task, s32 arg1, ActorShared80169f74Placement* placement);

#endif // ACTOR_312200_H
