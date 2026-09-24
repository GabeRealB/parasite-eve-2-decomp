#include "common.h"

#include "main/task.h"

/// State table of the room's message-driven task: publish the message table,
/// idle, then kill the task.
extern const TaskFuncTable3 D_shelter_b3_elevator_hall_8017D5F0;

/// Runs the message-driven task's current state through a stack copy of its
/// state table.
void func_shelter_b3_elevator_hall_8017DE18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_elevator_hall_8017D5F0;
    sp.funcs[task->state](task);
}
