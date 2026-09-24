#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b2_main_corridor.h"

/// Runs one tick of the room task through the three-state table
/// `D_shelter_b2_main_corridor_8017D5F0`, copying the table onto the stack and
/// calling the entry for the task's current state.
void func_shelter_b2_main_corridor_8017E338(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_main_corridor_8017D5F0;
    sp.funcs[task->state](task);
}
