#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_80062735;

/// The room's message table, which the room task answers messages with.
extern GpMsgEntry D_mine_tunnel_entrance_8017DAF0[];

/// State 0 of the room task: installs the room's message table, publishes the
/// task in pointer slot 7, advances to the next state and sets `D_80062735`.
void func_mine_tunnel_entrance_8017D644(Task* arg0)
{
    arg0->msgTable = D_mine_tunnel_entrance_8017DAF0;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80062735  = 1;
}
