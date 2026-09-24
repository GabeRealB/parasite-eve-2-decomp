#include "common.h"

#include "main/task.h"

/// State table of the room's message-driven task, indexed by `Task::state`.
extern const TaskFuncTable3 D_neo_ark_south_promenade_8017D5C4;

/// Dispatches the room's message-driven task through its three-state table,
/// copied onto the stack before the call.
void func_neo_ark_south_promenade_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_south_promenade_8017D5C4;
    sp.funcs[task->state](task);
}
