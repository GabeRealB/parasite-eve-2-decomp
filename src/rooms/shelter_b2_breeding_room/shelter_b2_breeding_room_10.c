#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b2_breeding_room.h"

/// Runs the handler for the task's state from the room's three-entry state
/// table, copied onto the stack first.
void func_shelter_b2_breeding_room_8017D840(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_breeding_room_8017D5C4;
    sp.funcs[task->state](task);
}
