#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// `Task::exitCallback` of the actor's main task, and the third entry of its
/// state table: hands the `GpEnemy` the spawn left in `Task::spawnArg2` back to
/// `Gp_DestroyEnemy`.
void func_actor_503500_801324C4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
