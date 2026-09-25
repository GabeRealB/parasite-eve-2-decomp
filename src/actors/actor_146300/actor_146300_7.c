#include "common.h"

#include "actors/actor_146300.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `Task::exitCallback` the spawn routine installs: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`.
void func_actor_146300_801327A4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
