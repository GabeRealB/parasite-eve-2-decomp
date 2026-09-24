#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/shelter_b1_transfer_tunnel.h"

/// State 0 of the room's task: installs the room's message table, publishes
/// the task in pointer slot 7 and advances to state 1.
void func_shelter_b1_transfer_tunnel_8017D62C(Task* task)
{
    task->msgTable = D_shelter_b1_transfer_tunnel_801828C0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
