#include "common.h"

#include "main/task.h"

/// The room task's three states: set up, idle, then `taskKill`.
extern const TaskFuncTable3 D_dryfield_night_r08_8017D5C4;

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_night_r08_8017D6C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_r08_8017D5C4;
    sp.funcs[task->state](task);
}
