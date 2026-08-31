#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc D_mine_forked_tunnel_80183104;

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

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_3", func_mine_forked_tunnel_8017E1E8);
