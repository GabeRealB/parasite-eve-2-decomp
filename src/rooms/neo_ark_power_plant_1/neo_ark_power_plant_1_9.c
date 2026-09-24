#include "common.h"

#include "main/task.h"

/// State table of the room task: `func_neo_ark_power_plant_1_8017D928`
/// installs the message table, `func_neo_ark_power_plant_1_8017D5EC` runs the
/// plant every frame, and the last state kills the task.
extern const TaskFuncTable3 D_neo_ark_power_plant_1_8017D5C4;

/// Runs the room task's current state: the handler `Task::state` selects from
/// `D_neo_ark_power_plant_1_8017D5C4`, copied onto the stack before the call.
void func_neo_ark_power_plant_1_8017D9C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_power_plant_1_8017D5C4;
    sp.funcs[task->state](task);
}
