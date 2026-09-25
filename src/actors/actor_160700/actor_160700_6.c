#include "common.h"

#include "actors/actor_160700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Exit callback: hands the task's `GpEnemy` back to `Gp_DestroyEnemy`.
void func_actor_160700_80132414(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
