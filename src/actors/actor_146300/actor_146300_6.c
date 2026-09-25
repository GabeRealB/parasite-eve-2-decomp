#include "common.h"

#include "actors/actor_146300.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The actor's task handler: publishes the task's work block in
/// `D_actor_146300_80142828` on the way through, then runs the handler its
/// state selects from a table built on the stack - the spawn routine for state
/// 0, the per-frame update after it.
void func_actor_146300_801326CC(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_146300_801324AC,
        func_actor_146300_80132728,
    };

    D_actor_146300_80142828 = task->work;
    fns[task->state](task->spawnArg2, task);
}
