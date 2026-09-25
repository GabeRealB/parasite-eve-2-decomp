#include "common.h"

#include "actors/actor_151000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Exit callback the spawn handler installs on the enemy's task: tears down
/// the enemy the task was spawned for.
void func_actor_151000_801324D4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
