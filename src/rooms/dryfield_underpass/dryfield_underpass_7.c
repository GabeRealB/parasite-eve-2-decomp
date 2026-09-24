#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_dryfield_underpass_8017D5C4;

/// Room task: runs the state handler `D_dryfield_underpass_8017D5C4` names for
/// `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_underpass_8017DAC8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_underpass_8017D5C4;
    sp.funcs[task->state](task);
}
