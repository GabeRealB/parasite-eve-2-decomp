#include "common.h"

#include "main/task.h"

#include "rooms/shelter_b6_training_room.h"

/// Runs a task through the room's three-state table, copied onto the stack
/// first and indexed by the task's state.
void func_shelter_b6_training_room_8017D8E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_training_room_8017D5C4;
    sp.funcs[task->state](task);
}
