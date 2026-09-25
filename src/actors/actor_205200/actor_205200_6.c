#include "common.h"

#include "actors/actor_205200.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Update of the actor's controller task: dispatches on its state to the
/// setup handler `func_actor_205200_8014A72C` (state 0) or the per-frame
/// handler `func_actor_205200_8014A958` (state 1), passing the task's enemy
/// record along with the task.
void func_actor_205200_8014B8C0(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_205200_8014A72C,
        (void (*)(GpEnemy*, Task*))func_actor_205200_8014A958,
    };

    fns[task->state](task->spawnArg2, task);
}
