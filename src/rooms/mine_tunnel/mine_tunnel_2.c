#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

/// The room's `GpMsgEntry` table: its 0x13F1 entry is `Room_Util02`, 0x13EF is
/// `func_mine_tunnel_8017D670` and 0x13F0 `func_mine_tunnel_8017D630`.
extern GpMsgEntry D_mine_tunnel_8017DFC4[];

/// State 0 of the tunnel's message-driven task family: park the room's
/// `GpMsgEntry` table in `Task::field_24`, publish the task in pointer slot 7,
/// and -- when the session is in the stage that owns this tunnel and flag 0xA1
/// is set -- hand `Room_Util36` the script argument 2. Then arm the
/// message flag and step the task on one state.
void func_mine_tunnel_8017D6EC(Task* arg0)
{
    arg0->field_24 = D_mine_tunnel_8017DFC4;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->field_9 == 1) && (GameFlag_GetNibble(0xA1) == 1)) {
        Room_Util36(2);
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80062735  = 1;
}

void func_mine_tunnel_8017D774(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/mine_tunnel/mine_tunnel_2", D_mine_tunnel_8017D5E4);
