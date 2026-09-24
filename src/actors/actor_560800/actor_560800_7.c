#include "common.h"

#include "main/task.h"

/// Task handler in the actor's task descriptor table that only kills the task.
void func_actor_560800_80136A88(Task* task)
{
    taskKill(task);
}
