#include "common.h"

#include "main/task.h"

/// The room event task's three states.
extern const TaskFuncTable3 D_mine_tunnel_8017D5C4;

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_mine_tunnel_8017D77C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}
