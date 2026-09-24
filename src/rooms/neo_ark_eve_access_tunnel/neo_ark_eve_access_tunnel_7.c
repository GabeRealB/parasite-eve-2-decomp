#include "common.h"

#include "main/task.h"

/// The room task's three states: message-table setup, per-frame view update,
/// and `taskKill`.
extern const TaskFuncTable3 D_neo_ark_eve_access_tunnel_8017D688;

/// Runs the task's current state through a stack copy of the room's
/// three-entry state table.
void func_neo_ark_eve_access_tunnel_8017E038(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_eve_access_tunnel_8017D688;
    sp.funcs[task->state](task);
}
