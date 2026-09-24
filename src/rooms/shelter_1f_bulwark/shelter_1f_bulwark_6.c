#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_shelter_1f_bulwark_8017D5D8;

/// The room's controller task: copies its three-entry state table (set up,
/// idle, kill) to the stack and runs the entry for the current state.
void func_shelter_1f_bulwark_8017DC20(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_bulwark_8017D5D8;
    sp.funcs[task->state](task);
}
