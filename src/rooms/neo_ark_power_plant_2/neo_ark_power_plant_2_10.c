#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_neo_ark_power_plant_2_8017D5C4;

/// Dispatches the room's message-driven task through its three-state table
/// `D_neo_ark_power_plant_2_8017D5C4`, copied onto the stack before the call.
void func_neo_ark_power_plant_2_8017D854(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_power_plant_2_8017D5C4;
    sp.funcs[task->state](task);
}
