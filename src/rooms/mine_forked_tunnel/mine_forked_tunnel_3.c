#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc   D_mine_forked_tunnel_80183104;
extern GpMsgEntry D_mine_forked_tunnel_80181C80[];
extern u8         D_80062735;

void func_mine_forked_tunnel_8017E48C(s32 arg0);

s32 func_mine_forked_tunnel_8017E134(s32 arg0, s32 arg1, s32 arg2)
{
    if ((arg2 == 2) && (Gp_GetCurBit2Flag(1) == 1)) {
        if (GameFlag_GetNibble(0x152) == 0) {
            Gp_RunCapCmd1(5);
        } else {
            Task_SpawnFromTable(&D_mine_forked_tunnel_80183104, 1, 0, 0);
        }
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_3", func_mine_forked_tunnel_8017E19C);

/// State 0 of the room's message-driven task family: park the room's
/// `GpMsgEntry` table in `Task::field_24`, publish the task in pointer slot 7,
/// arm the message flag, then hand off to `func_mine_forked_tunnel_8017E48C`.
void func_mine_forked_tunnel_8017E1E8(Task* arg0)
{
    arg0->field_24 = D_mine_forked_tunnel_80181C80;
    Game_SetPtrSlot(arg0, 7);
    D_80062735 = 1;
    func_mine_forked_tunnel_8017E48C(Gp_GetCurBit2Flag(1) == 2);
    arg0->state = (s32)(arg0->state + 1);
}
