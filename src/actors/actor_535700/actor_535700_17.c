#include "common.h"

#include "actors/actor_535700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The second enemy's task body: runs the handler for the task's state from a
/// table built on the stack - the spawn handler `func_actor_535700_80132B58`,
/// then the per-frame `func_actor_535700_80132F74`.
void func_actor_535700_80132F20(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_535700_80132B58,
        func_actor_535700_80132F74,
    };

    fns[task->state](task->spawnArg2, task);
}
