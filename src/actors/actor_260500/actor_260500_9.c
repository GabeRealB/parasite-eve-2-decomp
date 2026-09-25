#include "common.h"

#include "actors/actor_260500.h"
#include "gameplay/1BC.h"

/// `Task::exitCallback` the spawn routine installs: hands the task's `GpEnemy`
/// back to `Gp_DestroyEnemy`.
void func_actor_260500_8014A540(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
