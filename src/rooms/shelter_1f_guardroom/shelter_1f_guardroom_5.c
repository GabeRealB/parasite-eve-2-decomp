#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_shelter_1f_guardroom_8017D5C4;

/// The room's event task: copies its state table onto the stack and calls the
/// entry for the current state.
void func_shelter_1f_guardroom_8017D880(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_guardroom_8017D5C4;
    sp.funcs[task->state](task);
}
