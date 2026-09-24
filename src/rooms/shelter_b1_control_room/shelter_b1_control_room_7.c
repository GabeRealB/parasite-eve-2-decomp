#include "common.h"

#include "main/task.h"

/// State table of the task `func_shelter_b1_control_room_8017EECC` drives.
extern const TaskFuncTable3 D_shelter_b1_control_room_8017D5C4;

/// Runs the handler for the task's state from a stack copy of
/// `D_shelter_b1_control_room_8017D5C4`.
void func_shelter_b1_control_room_8017EECC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_control_room_8017D5C4;
    sp.funcs[task->state](task);
}
