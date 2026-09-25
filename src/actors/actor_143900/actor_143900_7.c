#include "common.h"

#include "actors/actor_143900.h"
#include "main/task.h"

/// Two-state dispatcher of the first variant: publishes the task's work block
/// in `D_actor_143900_801496B8` on the way through, then calls the handler its
/// state selects from a table built on the stack.
void func_actor_143900_80132324(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_143900_80131E70,
        func_actor_143900_80132380,
    };

    D_actor_143900_801496B8 = (Actor143900Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}
