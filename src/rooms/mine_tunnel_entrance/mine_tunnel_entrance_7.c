#include "common.h"

#include "main/task.h"

/// State handlers of the room task: set-up, two per-state handlers and
/// `taskKill`.
extern TaskFuncTable4 D_mine_tunnel_entrance_8017D5C4;

/// Per-frame entry of the room task: copies the state table onto the stack
/// and runs the handler for the task's current state.
void func_mine_tunnel_entrance_8017D6BC(Task* task)
{
    TaskFuncTable4 states;

    states = D_mine_tunnel_entrance_8017D5C4;
    states.funcs[task->state](task);
}
