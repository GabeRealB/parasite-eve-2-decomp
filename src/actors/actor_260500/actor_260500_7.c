#include "common.h"

#include "actors/actor_260500.h"

/// Two-state task handler: publishes the task's work block in
/// `D_actor_260500_80159E4C` on the way through, then calls the spawn routine
/// or the per-frame state, whichever `Task::state` selects from a table built
/// on the stack.
void func_actor_260500_8014A460(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_260500_80149FB0,
        func_actor_260500_8014A4BC,
    };

    D_actor_260500_80159E4C = task->work;
    fns[task->state](task->spawnArg2, task);
}
