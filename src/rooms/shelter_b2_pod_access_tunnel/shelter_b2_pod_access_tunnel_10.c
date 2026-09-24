#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b2_pod_access_tunnel.h"

/// Runs one tick of a room task through the three-state table
/// `D_shelter_b2_pod_access_tunnel_8017D5D8`, copying the table onto the stack
/// and calling the entry for the task's current state.
void func_shelter_b2_pod_access_tunnel_8017DC14(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_pod_access_tunnel_8017D5D8;
    sp.funcs[task->state](task);
}
