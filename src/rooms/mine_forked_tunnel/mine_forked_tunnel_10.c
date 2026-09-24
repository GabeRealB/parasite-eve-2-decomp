#include "common.h"

#include "main/task.h"

/// State table of the room's message-driven task, indexed by `Task::state`.
extern const TaskFuncTable3 D_mine_forked_tunnel_8017D5DC;

/// Dispatches the room's message-driven task through its three-state table,
/// copied onto the stack before the call.
void func_mine_forked_tunnel_8017E25C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_forked_tunnel_8017D5DC;
    sp.funcs[task->state](task);
}
