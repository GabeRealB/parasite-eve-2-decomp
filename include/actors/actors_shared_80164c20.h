#ifndef ACTORS_SHARED_80164C20_H
#define ACTORS_SHARED_80164C20_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block these enemies hang off `Task::idMap`; only the flag
/// this body tests is known. It sits where `Actor00100Work::field_4` does, and
/// is read the same way, by the sibling body `Actor00100_Fn0B4D8`.
typedef struct ActorsShared80164c20Work {
    /* 0x000 */ byte pad_0[4];
    /* 0x004 */ s16  field_4;
} ActorsShared80164c20Work;

/// State-0 handler of the table at `actor_323000.rodata` offset 4, reached as
/// `fns[state](task->spawnArg2, task)` like the other handler tables in this
/// family - hence the enemy/task pair. Shared by `actor_323000` and
/// `actor_323400`, whose tables carry it at the same index.
void ActorsShared80164c20(GpEnemy* arg0, Task* arg1);

#endif
