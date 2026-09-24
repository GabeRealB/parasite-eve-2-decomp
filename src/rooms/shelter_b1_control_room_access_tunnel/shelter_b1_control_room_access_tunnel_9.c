#include "common.h"

#include "main/task.h"

/// State handlers of the room's task: the setup state, an idle state and
/// `taskKill`.
extern const TaskFuncTable3 D_shelter_b1_control_room_access_tunnel_8017D5C4;

/// Runs the room's task through its three-state handler table, copied onto
/// the stack before the call.
void func_shelter_b1_control_room_access_tunnel_8017D68C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_control_room_access_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}
