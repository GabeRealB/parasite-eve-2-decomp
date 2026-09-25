#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/1BC.h"

/// `Task::exitCallback` of the first variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`.
void func_actor_143900_80132404(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
