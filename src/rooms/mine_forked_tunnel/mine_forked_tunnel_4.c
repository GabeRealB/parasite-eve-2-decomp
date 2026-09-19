#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/mine_forked_tunnel.h"

extern s32 D_mine_forked_tunnel_801831AC;
extern s32 D_mine_forked_tunnel_801834F4;

void func_mine_forked_tunnel_8017E48C(s32 arg0);

void func_mine_forked_tunnel_8017E2B4(void)
{
    SndEvt_EnqueueTypeA(0x54070005, 0, 0);
}

void func_mine_forked_tunnel_8017E2E0(Task* arg0)
{
    s32 state;

    state = arg0->state;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(1);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (Gp_GetCapEventKey() == state) {
                    func_800E8634((s32)&D_mine_forked_tunnel_801831AC, 0, (s32)&D_mine_forked_tunnel_801834F4);
                    GameFlag_SetNibble(0x75, 1);
                }
                taskKill(arg0);
            }
            break;
    }
}

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
                taskKill(arg0);
            }
            break;
    }
}

void func_mine_forked_tunnel_8017E48C(s32 arg0)
{
    GpAreaKey*               sess;
    MineForkedTunnelSprtRec* rec;
    MineForkedTunnelViewB*   v28;
    MineForkedTunnelViewA*   v34;

    sess = &gGameSession->at4.loc;
    rec  = (MineForkedTunnelSprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];

    if (!(arg0 & 0xFF)) {
        v28           = rec->field_28;
        v28->field_2C = 0;
        v34           = rec->field_34;
        v34->field_1C = 0;
        return;
    }
    v28           = rec->field_28;
    v28->field_2C = 1;
    v34           = rec->field_34;
    v34->field_1C = 1;
}
