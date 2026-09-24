#include "common.h"

#include "main/task.h"

/// The room task's three-state table: install the message table and spawn the
/// room's tasks, idle, then kill.
extern const TaskFuncTable3 D_shelter_b4_water_supply_8017D5D8;

/// The room task: copies the three-state table
/// `D_shelter_b4_water_supply_8017D5D8` onto the stack and runs the entry for
/// the task's current state.
void func_shelter_b4_water_supply_8017DDA4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_water_supply_8017D5D8;
    sp.funcs[task->state](task);
}
