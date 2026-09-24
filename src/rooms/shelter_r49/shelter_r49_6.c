#include "common.h"

#include "main/task.h"

/// The room task's states: set up, idle, then `taskKill`.
extern const TaskFuncTable3 D_shelter_r49_8017D5C4;

/// The room task: copies its three-state table to the stack and runs the
/// entry the task's state selects.
void func_shelter_r49_8017D6C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r49_8017D5C4;
    sp.funcs[task->state](task);
}
