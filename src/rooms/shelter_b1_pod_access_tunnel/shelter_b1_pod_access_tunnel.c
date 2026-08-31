#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc D_shelter_b1_pod_access_tunnel_80181108;

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", D_shelter_b1_pod_access_tunnel_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", func_shelter_b1_pod_access_tunnel_8017D61C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", func_shelter_b1_pod_access_tunnel_8017D7B4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", D_shelter_b1_pod_access_tunnel_8017D5D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", func_shelter_b1_pod_access_tunnel_8017DA74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", func_shelter_b1_pod_access_tunnel_8017DC18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", func_shelter_b1_pod_access_tunnel_8017DD68);

s32 func_shelter_b1_pod_access_tunnel_8017DD70(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x12F) != 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80181108, 0, 0, 0);
        } else {
            GameFlag_SetNibble(0x12F, 1);
            Gp_RunCapCmd1(0xA);
        }
    }
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDD8(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", func_shelter_b1_pod_access_tunnel_8017DDE0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", func_shelter_b1_pod_access_tunnel_8017DE10);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", D_shelter_b1_pod_access_tunnel_8017D610);
