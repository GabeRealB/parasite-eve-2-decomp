#include "common.h"

#include "actors/actors_shared_80131f9c.h"

/// Two-state dispatcher whose handler table is built on the stack, publishing
/// the task's work block in `ActorsShared80131f9cWork` on the way through so
/// the rest of the overlay can reach it without the task.
void ActorsShared80131f9c(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        ActorsShared80131f9cSub0,
        ActorsShared80131f9cSub1,
    };

    ActorsShared80131f9cWork = task->idMap;
    fns[task->state](task->spawnArg2, task);
}
