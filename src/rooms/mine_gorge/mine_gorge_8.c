#include "common.h"

#include "main/task.h"

/// The room task's three state handlers, in the room's leading rodata.
extern const TaskFuncTable3 D_mine_gorge_8017D5C4;

/// Runs the room task's current state from `D_mine_gorge_8017D5C4`, through a
/// copy of the table on the stack.
void func_mine_gorge_8017D9A0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_gorge_8017D5C4;
    sp.funcs[task->state](task);
}
