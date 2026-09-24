#include "common.h"

#include "main/task.h"

/// States of the room's message task: install the message table, idle, die.
extern const TaskFuncTable3 D_shelter_b1_main_corridor_8017D5F0;

/// Runs the room's message task: calls the state handler `task->state` selects
/// from a stack copy of its three-entry table.
void func_shelter_b1_main_corridor_8017DD98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_main_corridor_8017D5F0;
    sp.funcs[task->state](task);
}
