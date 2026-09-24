#include "common.h"

#include "main/task.h"

/// The three states of the task below.
extern const TaskFuncTable3 D_shelter_b1_underground_parking_8017D7F4;

/// Dispatches a task through the three-entry state table, copied onto the
/// stack first.
void func_shelter_b1_underground_parking_801838B4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_underground_parking_8017D7F4;
    sp.funcs[task->state](task);
}
