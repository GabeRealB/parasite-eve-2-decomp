#include "common.h"

#include "actors/actor_120400.h"

/// Exit callback of the parent task, installed by its spawn handler: runs the
/// common enemy teardown.
void func_actor_120400_801327B4(Task* task)
{
    Gp_EnemyTaskExit(task);
}
