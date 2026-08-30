#include "common.h"

#include "gameplay/3CD8.h"

#include "main/task.h"

void func_mine_forked_tunnel_8017E48C(s32 arg0);

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_4", func_mine_forked_tunnel_8017E25C);

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_4", func_mine_forked_tunnel_8017E2B4);

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_4", func_mine_forked_tunnel_8017E2E0);

void func_mine_forked_tunnel_8017E38C(Task* arg0)
{
    s32 state;
    s16 temp;

    state = arg0->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(2, 0, 0);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            temp                = arg0->killCountdown + 1;
            arg0->killCountdown = temp;
            if (temp >= 0xB) {
                if (Gp_GetCapEventKey() == state) {
                    Gp_StartCapSlot(2, 0, 1);
                    func_mine_forked_tunnel_8017E48C(1);
                }
                Gp_MsgPlayerWeapon(1);
                Task_Kill(arg0);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_4", func_mine_forked_tunnel_8017E48C);
