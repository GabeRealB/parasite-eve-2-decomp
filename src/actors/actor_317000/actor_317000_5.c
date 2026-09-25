#include "common.h"

#include "actors/actor_317000.h"

#include "main/task.h"

/// Task callback of the actor: copies the three-handler table
/// `D_actor_317000_80161E24` (spawn `func_actor_317000_8016267C`, per-frame
/// tick `func_actor_317000_80161E68`, exit `func_actor_317000_80162724`) onto
/// the stack and runs the entry `Task::state` selects.
void func_actor_317000_80162624(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_317000_80161E24;
    sp.funcs[task->state](task);
}
