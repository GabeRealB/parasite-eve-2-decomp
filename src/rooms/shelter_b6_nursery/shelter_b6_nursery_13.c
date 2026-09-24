#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b6_nursery.h"

/// Runs the room's message task: calls the state handler `task->state` selects
/// from a stack copy of its three-entry table.
void func_shelter_b6_nursery_8017FF9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_nursery_8017D6A4;
    sp.funcs[task->state](task);
}
