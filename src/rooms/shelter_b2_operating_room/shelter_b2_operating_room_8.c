#include "common.h"

#include "main/task.h"

/// State handlers of the task `func_shelter_b2_operating_room_8017DD60` drives.
extern const TaskFuncTable3 D_shelter_b2_operating_room_8017D5F0;

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_b2_operating_room_8017D5F0`.
void func_shelter_b2_operating_room_8017DD60(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_operating_room_8017D5F0;
    sp.funcs[task->state](task);
}
