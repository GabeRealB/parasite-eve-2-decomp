#include "common.h"

#include "actors/actor_161500.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The actor's `Task::exitCallback`: hands the task's `GpEnemy`, parked in
/// `Task::spawnArg2`, back to `Gp_DestroyEnemy`.
void func_actor_161500_8013284C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
