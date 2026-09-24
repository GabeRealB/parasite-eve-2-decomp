#include "common.h"

#include "main/task.h"

/// The room task's three states: entry, idle and `taskKill`.
extern const TaskFuncTable3 D_dryfield_toilet_8017D5C4;

/// The room task's update: runs the handler for its current state from a stack
/// copy of the room's state table.
void func_dryfield_toilet_8017D9E4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_toilet_8017D5C4;
    sp.funcs[task->state](task);
}
