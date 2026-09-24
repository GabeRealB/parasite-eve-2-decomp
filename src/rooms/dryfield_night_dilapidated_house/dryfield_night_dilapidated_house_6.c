#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_dilapidated_house.h"

/// Runs the room task's current state, through a copy of its state table
/// taken onto the stack.
void func_dryfield_night_dilapidated_house_8017DA18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_dilapidated_house_8017D5DC;
    sp.funcs[task->state](task);
}
