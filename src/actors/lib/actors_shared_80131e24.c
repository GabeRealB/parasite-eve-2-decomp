#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Two-state dispatcher whose handler table is built on the stack. Unlike
/// `ActorsShared8013845c`, the handlers take the task's `GpEnemy` as well as
/// the task; each overlay aliases its own state-0 / state-1 callbacks as
/// ActorsShared80131e24Sub0 / ActorsShared80131e24Sub1.
void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task);
void ActorsShared80131e24Sub1(GpEnemy* enemy, Task* task);

void ActorsShared80131e24(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        ActorsShared80131e24Sub0,
        ActorsShared80131e24Sub1,
    };

    fns[task->state](task->spawnArg2, task);
}
