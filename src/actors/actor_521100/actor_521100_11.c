#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// State handlers of the task `func_actor_521100_80135AE4` runs: the setup
/// `func_actor_521100_80135B40`, the per-frame tick
/// `func_actor_521100_80135B80` and `Gp_DestroyEnemy`.
extern GpEnemyTaskFuncTable3 D_actor_521100_80131E40;

/// Task body of the actor's second part: copies `D_actor_521100_80131E40`
/// onto the stack and runs the handler for `Task::state` on the task's
/// `GpEnemy`.
void func_actor_521100_80135AE4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_521100_80131E40;
    sp.funcs[task->state](task->spawnArg2, task);
}
