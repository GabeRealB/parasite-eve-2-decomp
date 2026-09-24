#include "common.h"

#include "main/task.h"

/// The three-state handler table `func_mine_mesa_8017DD98` dispatches through.
extern const TaskFuncTable3 D_mine_mesa_8017D5D8;

void func_mine_mesa_8017DD98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_mesa_8017D5D8;
    sp.funcs[task->state](task);
}
