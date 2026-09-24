#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_night_water_tower.h"

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_water_tower_8017DB28(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_tower_8017D5DC;
    sp.funcs[task->state](task);
}
