#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_saloon_g_r.h"

/// The room task: copies the three-state table
/// `D_dryfield_night_saloon_g_r_8017D5DC` onto the stack and runs the entry
/// for the task's current state - the entry tick, the idle state, then
/// `taskKill`.
void func_dryfield_night_saloon_g_r_8017E050(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_saloon_g_r_8017D5DC;
    sp.funcs[task->state](task);
}
