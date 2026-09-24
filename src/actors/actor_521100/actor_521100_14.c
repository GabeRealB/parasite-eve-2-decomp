#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// `Task::exitCallback` the create state `func_actor_521100_80135DDC`
/// installs: hands the task's `GpEnemy` back to `Gp_DestroyEnemy`.
void func_actor_521100_801366FC(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
