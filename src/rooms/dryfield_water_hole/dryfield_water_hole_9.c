#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_water_hole.h"

/// The room task: copies the three-state table
/// `D_dryfield_water_hole_8017D5C4` onto the stack and runs the entry for the
/// task's current state - the entry tick, the idle state, then `taskKill`.
void func_dryfield_water_hole_8017D840(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_hole_8017D5C4;
    sp.funcs[task->state](task);
}
