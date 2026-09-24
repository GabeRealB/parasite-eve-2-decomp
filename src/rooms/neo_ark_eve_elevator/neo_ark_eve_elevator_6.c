#include "common.h"

#include "main/task.h"

/// The event task's three states: install the message table, idle, and kill.
extern const TaskFuncTable3 D_neo_ark_eve_elevator_8017D5C4;

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_neo_ark_eve_elevator_8017D6C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_eve_elevator_8017D5C4;
    sp.funcs[task->state](task);
}
