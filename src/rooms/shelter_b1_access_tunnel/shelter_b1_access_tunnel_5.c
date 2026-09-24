#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "main/task.h"
#include "rooms/shelter_b1_access_tunnel.h"

void func_shelter_b1_access_tunnel_8017DCBC(Task* task)
{
    task->msgTable = D_shelter_b1_access_tunnel_8017E71C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
