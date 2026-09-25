#include "common.h"

#include "actors/actor_323000.h"
#include "gameplay/1BC.h"

/// `Task::exitCallback` the spawn handler installs: destroys the enemy the
/// task carries.
void func_actor_323000_80164B18(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
