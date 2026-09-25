#include "common.h"

#include "main/task.h"

/// Exit callback the rig controller installs at `Task::exitCallback`, and the
/// third entry of its state table: kills the task.
void func_actor_303600_80162850(Task* task)
{
    taskKill(task);
}
