#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_shelter_b6_growth_room_8017D5C4;

/// Steps a task through the room's three-state table (set-up, idle, kill): the
/// table is copied onto the stack and the entry for the current state is
/// called.
void func_shelter_b6_growth_room_8017D7D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_growth_room_8017D5C4;
    sp.funcs[task->state](task);
}
