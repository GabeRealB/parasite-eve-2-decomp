#include "common.h"

#include "main/task.h"

/// State handlers of the room's controller task.
extern const TaskFuncTable3 D_shelter_1f_heliport_8017D710;

/// Runs the task's current state through its three-entry state table, copied
/// onto the stack before the call.
void func_shelter_1f_heliport_80180768(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_heliport_8017D710;
    sp.funcs[task->state](task);
}
