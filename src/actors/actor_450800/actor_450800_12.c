#include "common.h"

#include "gameplay/1BC.h"

#include "actors/actor_450800.h"

/// Exit callback of the enemy's task, set by its spawn handler
/// `func_actor_450800_80132E9C`: releases the enemy slot the task was spawned
/// for.
void func_actor_450800_8013333C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
