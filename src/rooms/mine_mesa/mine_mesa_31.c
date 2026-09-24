#include "common.h"

#include "main/task.h"

/// The four-state handler table `func_mine_mesa_80181894` dispatches through.
extern TaskFuncTable4 D_mine_mesa_8017D660;

void func_mine_mesa_80181894(Task* task)
{
    TaskFuncTable4 states;

    states = D_mine_mesa_8017D660;
    states.funcs[task->state](task);
}
