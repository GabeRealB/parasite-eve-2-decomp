#include "common.h"

#include "actors/actor_461800.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `Task::exitCallback` of the second variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`.
void func_actor_461800_80133634(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
