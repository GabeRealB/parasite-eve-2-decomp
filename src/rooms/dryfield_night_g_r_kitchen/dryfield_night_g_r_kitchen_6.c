#include "common.h"

#include "main/task.h"

/// The room entry task's three states: install the room's message table,
/// idle, and `taskKill`.
extern const TaskFuncTable3 D_dryfield_night_g_r_kitchen_8017D5DC;

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_g_r_kitchen_8017D9A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_g_r_kitchen_8017D5DC;
    sp.funcs[task->state](task);
}
