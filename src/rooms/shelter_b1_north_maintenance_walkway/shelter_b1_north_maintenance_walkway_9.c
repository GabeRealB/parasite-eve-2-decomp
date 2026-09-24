#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_shelter_b1_north_maintenance_walkway_8017D5D8;

/// The room task. Runs the handler for its current state from the room's
/// three-entry state table: set-up, an idle tick, and `taskKill`.
void func_shelter_b1_north_maintenance_walkway_8017DAFC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_north_maintenance_walkway_8017D5D8;
    sp.funcs[task->state](task);
}
