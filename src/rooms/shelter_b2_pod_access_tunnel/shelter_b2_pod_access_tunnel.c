#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_b2_pod_access_tunnel_80183BCC[];
extern TaskDesc   D_shelter_b2_pod_access_tunnel_80183BFC;

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", func_shelter_b2_pod_access_tunnel_8017D62C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", func_shelter_b2_pod_access_tunnel_8017D7C4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", func_shelter_b2_pod_access_tunnel_8017D9A8);

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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", jtbl_shelter_b2_pod_access_tunnel_8017D5F8);
