#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The actor's teardown state and `Task::exitCallback`: hands the task to
/// `Gp_EnemyTaskExit`.
void func_actor_210700_8014A1E8(Task* task)
{
    Gp_EnemyTaskExit(task);
}
