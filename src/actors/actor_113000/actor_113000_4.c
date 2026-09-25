#include "common.h"

#include "actors/actor_113000.h"

#include "main/task.h"

/// Task callback of the actor: copies the three-handler table
/// `D_actor_113000_80131E24` (spawn `func_actor_113000_80131F90`, per-frame
/// tick `func_actor_113000_80132070`, exit `Gp_EnemyTaskExit`) onto the stack
/// and runs the entry `Task::state` selects.
void func_actor_113000_80131F38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113000_80131E24;
    sp.funcs[task->state](task);
}
