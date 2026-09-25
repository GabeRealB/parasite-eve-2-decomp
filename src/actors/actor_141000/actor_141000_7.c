#include "common.h"

#include "actors/actor_141000.h"

#include "main/task.h"

/// `Task::exitCallback` the model actor's spawn state installs: it only hands
/// the task to `Gp_EnemyTaskExit`.
void func_actor_141000_801339BC(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
