#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_b2_pod_access_tunnel_80183BCC[];
extern TaskDesc   D_shelter_b2_pod_access_tunnel_80183BFC;
extern u8         D_801153F4;
extern s16        D_80071076;

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", func_shelter_b2_pod_access_tunnel_8017D62C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", func_shelter_b2_pod_access_tunnel_8017D7C4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", RoomsShared8017d878Table);

void func_shelter_b2_pod_access_tunnel_8017D9A8(Task* task)
{
    s32 var_v0;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(GameFlag_GetNibble(0xFC) != 0 ? 3 : 1);
            D_801153F4 = 1;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                if (Gp_GetCapEventKey() == 1) {
                    GameFlag_SetNibble(0x1B6, 2);
                }
                D_801153F4 = 0;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            SndEvt_EnqueueType6(0x54230004, 0, 0);
            GameFlag_SetNibble(0x1B6, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x54230004);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area = 0x11;
            Mc_SaveData.at4.loc.warp = 3;
            Mc_SaveData.at4.loc.room = 1;
            D_80071076               = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b2_pod_access_tunnel_8017DB28(void)
{
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB30(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_shelter_b2_pod_access_tunnel_80183BFC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB70(void)
{
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB78(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        SndEvt_EnqueueType6(0x16, 0, 0);
    }
    return 0;
}

void func_shelter_b2_pod_access_tunnel_8017DBA8(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_pod_access_tunnel_80183BCC;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 0x16) {
        gGameSession->flowFlags = 3;
    }
    arg0->state = arg0->state + 1;
}

void func_shelter_b2_pod_access_tunnel_8017DC0C(void)
{
}
