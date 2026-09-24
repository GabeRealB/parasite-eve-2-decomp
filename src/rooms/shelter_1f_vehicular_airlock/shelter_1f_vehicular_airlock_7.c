#include "common.h"

#include "main/task.h"

/// The message task's three state handlers, in the room's leading rodata.
extern const TaskFuncTable3 D_shelter_1f_vehicular_airlock_8017D5D8;

/// Runs the room's message task through its three states: publishing the
/// room's message table (`func_shelter_1f_vehicular_airlock_8017D9FC`), idling
/// (`func_shelter_1f_vehicular_airlock_8017DA40`) and `taskKill`. The table is
/// copied onto the stack first, so the call goes through a local copy rather
/// than the rodata.
void func_shelter_1f_vehicular_airlock_8017DA48(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_vehicular_airlock_8017D5D8;
    sp.funcs[task->state](task);
}
