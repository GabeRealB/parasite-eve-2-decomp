#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Cutscene script blob handed to `func_800E8614`; unnamed in the gameplay
/// map, which keeps the raw address.
extern s32 D_mine_tunnel_8017E024;

/// The room's message table: 0x13EE is handled by `func_mine_tunnel_8017D5EC`,
/// 0x13F1 by `func_mine_tunnel_8017D5E4`, 0x13EF by `func_mine_tunnel_8017D670`
/// and 0x13F0 by `func_mine_tunnel_8017D630`.
extern GpMsgEntry D_mine_tunnel_8017DFC4[];

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_mine_tunnel_8017D5E4(void)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, passes both to `func_80179A04` and returns 1.
s32 func_mine_tunnel_8017D5EC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

s32 func_mine_tunnel_8017D630(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0x11A) >= 2 ? 3 : 2);
    }
    return 0;
}

s32 func_mine_tunnel_8017D670(s32 arg0, s32 arg1, RoomEventMsg* msg, s32 arg3)
{
    u8 temp_v1;

    temp_v1 = msg->field_2;
    if ((temp_v1 == 1) && (gGameSession->at4.loc.place == temp_v1) && (GameFlag_GetNibble(0xA1) == 0)) {
        GameFlag_SetNibble(0xA1, 1);
        Gp_MsgPlayerWeapon(0);
        func_800E8614((s32)&D_mine_tunnel_8017E024, 1);
    }
    return 0;
}

/// Stores its argument in `Gp_StateF0.field_1C`; the room's event task calls it with 2
/// on entry to the tunnel once flag 0xA1 is set.
void func_mine_tunnel_8017D6E0(s32 arg0)
{
    Gp_StateF0.field_1C = arg0;
}

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

/// State 1 of the room's event task: does nothing, so the task idles here.
void func_mine_tunnel_8017D774(Task* task)
{
}

/// The room event task's three states: install the message table, idle, and
/// kill.
const TaskFuncTable3 D_mine_tunnel_8017D5C4 = {
    {
        func_mine_tunnel_8017D6EC,
        func_mine_tunnel_8017D774,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_mine_tunnel_8017D77C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}
