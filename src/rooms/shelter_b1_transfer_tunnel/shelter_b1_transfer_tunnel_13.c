#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b1_transfer_tunnel.h"

/// Runs the room's task through its three-state handler table, copied onto
/// the stack before the call.
void func_shelter_b1_transfer_tunnel_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_transfer_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}
