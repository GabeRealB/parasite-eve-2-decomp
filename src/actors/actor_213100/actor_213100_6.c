#include "common.h"
#include "main/task.h"

/// The actor's teardown state and its `Task::exitCallback`: hands the task to
/// `Gp_EnemyTaskExit`.
void func_actor_213100_8014A21C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
