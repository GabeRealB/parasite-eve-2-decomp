#include "common.h"

#include "main/task.h"

/// The room's three-state task table, in the leading rodata.
extern const TaskFuncTable3 D_shelter_b1_elevator_hall_8017D5D8;

/// Runs a task through the room's three-state table, copied onto the stack
/// first and indexed by the task's state.
void func_shelter_b1_elevator_hall_8017DC28(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_elevator_hall_8017D5D8;
    sp.funcs[task->state](task);
}
