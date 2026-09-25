#include "common.h"

#include "actors/actor_141000.h"

#include "main/task.h"

/// `Task::exitCallback` the controller's spawn state installs: it only hands
/// the task to `taskKill`.
void func_actor_141000_80132E04(Task* task)
{
    taskKill(task);
}
