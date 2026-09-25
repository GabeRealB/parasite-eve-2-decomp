#include "common.h"

#include "actors/actor_110300.h"
#include "main/task.h"

/// The actor's task entry: a two-state dispatcher whose handler table is built
/// on the stack. It publishes the task's work block in
/// `D_actor_110300_8013A0A0` before calling the handler, which is how the
/// overlay's other functions reach the block without the task.
void func_actor_110300_80131F9C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_110300_80131E24,
        func_actor_110300_80132020,
    };

    D_actor_110300_8013A0A0 = task->work;
    fns[task->state](task->spawnArg2, task);
}
