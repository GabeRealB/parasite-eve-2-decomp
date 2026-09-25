#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// Exit callback the spawn handler installs, and the third state of the main
/// task: hands the task to `Gp_EnemyTaskExit`.
void func_actor_443500_801327A4(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
