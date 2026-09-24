#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_shelter_b2_septic_tank_8017D5D8;

/// The room task: copies the three-state table `D_shelter_b2_septic_tank_8017D5D8`
/// onto the stack and runs the entry for the task's current state.
void func_shelter_b2_septic_tank_8017DB10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_septic_tank_8017D5D8;
    sp.funcs[task->state](task);
}
