#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

#include "actors/actor_160600.h"

/// The actor's `Task::exitCallback`: hands the task's `GpEnemy`, parked in
/// `Task::spawnArg2`, back to `Gp_DestroyEnemy`.
void func_actor_160600_80132350(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
