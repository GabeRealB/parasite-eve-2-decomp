#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_dryfield_night_junk_yard_8017D5C4;

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_night_junk_yard_8017D960(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_junk_yard_8017D5C4;
    sp.funcs[task->state](task);
}
