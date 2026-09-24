#include "common.h"

#include "main/task.h"

/// The room task's three states: set up, wait for the stream to start the
/// named sequence, and `taskKill`.
extern const TaskFuncTable3 D_dryfield_junk_yard_8017D5C4;

/// The room's own task: copies its three-state table to the stack and runs
/// the entry the task's state selects.
void func_dryfield_junk_yard_8017DCB4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_junk_yard_8017D5C4;
    sp.funcs[task->state](task);
}
