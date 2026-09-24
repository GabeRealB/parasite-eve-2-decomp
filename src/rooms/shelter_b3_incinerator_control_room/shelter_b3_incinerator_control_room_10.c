#include "common.h"

#include "main/task.h"

/// The three state handlers `func_shelter_b3_incinerator_control_room_8017FCB8` dispatches
/// to.
extern const TaskFuncTable3 D_shelter_b3_incinerator_control_room_8017D6A4;

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_b3_incinerator_control_room_8017D6A4`.
void func_shelter_b3_incinerator_control_room_8017FCB8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_incinerator_control_room_8017D6A4;
    sp.funcs[task->state](task);
}
