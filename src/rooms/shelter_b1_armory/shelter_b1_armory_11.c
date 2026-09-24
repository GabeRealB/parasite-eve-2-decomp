#include "common.h"

#include "main/task.h"

/// State handlers of the room's controller task: installing its message table,
/// an idle tick, and the kill.
extern const TaskFuncTable3 D_shelter_b1_armory_8017D714;

/// Runs the task's current state through its three-entry state table, copied
/// onto the stack before the call.
void func_shelter_b1_armory_8018078C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_armory_8017D714;
    sp.funcs[task->state](task);
}
