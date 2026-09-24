#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_water_hole.h"

/// The room task: copies the three-state table
/// `D_dryfield_night_water_hole_8017D688` onto the stack and runs the entry for
/// the task's current state - the entry tick, the idle state, then `taskKill`.
void func_dryfield_night_water_hole_8017DE30(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_hole_8017D688;
    sp.funcs[task->state](task);
}
