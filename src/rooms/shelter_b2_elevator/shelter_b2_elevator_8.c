#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_shelter_b2_elevator_8017D5C4;

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_shelter_b2_elevator_8017DB18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_elevator_8017D5C4;
    sp.funcs[task->state](task);
}
