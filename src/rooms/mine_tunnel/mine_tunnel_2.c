#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/mine_tunnel.h"
#include "rooms/room_common.h"

/// The room's message table: 0x13EE is handled by `func_mine_tunnel_8017D5EC`,
/// 0x13F1 by `func_mine_tunnel_8017D5E4`, 0x13EF by `func_mine_tunnel_8017D670`
/// and 0x13F0 by `func_mine_tunnel_8017D630`.
extern GpMsgEntry D_mine_tunnel_8017DFC4[];

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and - when the session is at place 1
/// and flag 0xA1 is 1 - calls `func_mine_tunnel_8017D6E0` with 2. Then sets
/// `D_80062735` and advances to state 1.
void func_mine_tunnel_8017D6EC(Task* arg0)
{
    arg0->msgTable = D_mine_tunnel_8017DFC4;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 1) && (GameFlag_GetNibble(0xA1) == 1)) {
        func_mine_tunnel_8017D6E0(2);
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80062735  = 1;
}

void func_mine_tunnel_8017D774(void)
{
}
