#include "common.h"

#include "actors/actor_323400.h"
#include "gameplay/1BC.h"

/// `Task::exitCallback` the spawn handler installs: destroys the enemy the
/// task carries.
void func_actor_323400_80164A78(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
