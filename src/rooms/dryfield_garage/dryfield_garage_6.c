#include "common.h"
#include "main/task.h"

/// The room task's three-state table: the entry state
/// `func_dryfield_garage_8017DB18`, the idle state
/// `func_dryfield_garage_8017DC08`, then `taskKill`.
extern const TaskFuncTable3 D_dryfield_garage_8017D5DC;

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_garage_8017DC10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_garage_8017D5DC;
    sp.funcs[task->state](task);
}
