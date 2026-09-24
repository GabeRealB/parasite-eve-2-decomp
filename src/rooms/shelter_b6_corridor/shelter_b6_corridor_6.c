#include "common.h"

#include "main/task.h"

/// The room task's three states: set the room up, the per-frame state, end.
extern const TaskFuncTable3 D_shelter_b6_corridor_8017D5C4;

/// Runs the room task's current state from its three-entry table, which it
/// copies onto the stack before the call.
void func_shelter_b6_corridor_8017E144(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_corridor_8017D5C4;
    sp.funcs[task->state](task);
}
