#include "common.h"

#include "main/task.h"

/// The room task's three states, in the leading rodata: the entry tick
/// `func_dryfield_water_tank_8017D9D4`, the per-frame ambience
/// `func_dryfield_water_tank_8017DA4C`, then `taskKill`.
extern const TaskFuncTable3 D_dryfield_water_tank_8017D5C4;

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_water_tank_8017DAF0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_tank_8017D5C4;
    sp.funcs[task->state](task);
}
