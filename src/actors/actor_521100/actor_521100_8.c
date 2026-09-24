#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

void func_actor_521100_80131E8C(GpEnemy* enemy, Task* task);
void func_actor_521100_801353CC(GpEnemy* enemy, Task* task);

/// The actor's task body: runs the handler for `Task::state` out of a two-entry
/// table built on the stack - the spawn state `func_actor_521100_80131E8C`,
/// then the per-frame state `func_actor_521100_801353CC` - passing the task's
/// `GpEnemy` along with the task.
void func_actor_521100_80135378(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_521100_80131E8C,
        func_actor_521100_801353CC,
    };

    fns[task->state](task->spawnArg2, task);
}
