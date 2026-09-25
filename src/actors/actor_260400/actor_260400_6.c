#include "common.h"

#include "actors/actor_260400.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Two-state task handler: publishes the task's work block in
/// `D_actor_260400_80154C70` on the way through, then calls the spawn routine
/// or the per-frame state, whichever `Task::state` selects from a table built
/// on the stack.
void func_actor_260400_8014A550(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_260400_80149FE0,
        func_actor_260400_8014A5AC,
    };

    D_actor_260400_80154C70 = (Actor260400Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}
