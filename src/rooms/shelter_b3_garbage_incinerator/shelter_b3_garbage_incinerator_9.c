#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_shelter_b3_garbage_incinerator_8017D5C4;

/// Runs the room controller's current state through its three-entry state
/// table, copied onto the stack before the call.
void func_shelter_b3_garbage_incinerator_8017DC7C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_garbage_incinerator_8017D5C4;
    sp.funcs[task->state](task);
}
