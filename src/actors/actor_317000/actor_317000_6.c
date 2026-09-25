#include "common.h"

#include "main/task.h"

/// Exit handler, both the third entry of `D_actor_317000_80161E24` and the
/// `Task::exitCallback` `func_actor_317000_8016267C` installs: ends the task
/// through `Gp_EnemyTaskExit`.
void func_actor_317000_80162724(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
