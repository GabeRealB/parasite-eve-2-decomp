#include "common.h"
#include "main/task.h"
extern TaskDesc D_shelter_b1_pod_access_tunnel_80182D2C;
extern TaskDesc D_801348D8;

void func_shelter_b1_pod_access_tunnel_8017E41C(s32 arg0)
{
    Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80182D2C, 0, arg0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_3", func_shelter_b1_pod_access_tunnel_8017E44C);

void func_shelter_b1_pod_access_tunnel_8017E52C(s32 arg0)
{
    Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80182D2C, 1, arg0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_3", func_shelter_b1_pod_access_tunnel_8017E55C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_3", func_shelter_b1_pod_access_tunnel_8017E5B4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_3", func_shelter_b1_pod_access_tunnel_8017E66C);

void func_shelter_b1_pod_access_tunnel_8017E704(void)
{
    Task_SpawnFromTable(&D_801348D8, 0, 0, 0);
}

void func_shelter_b1_pod_access_tunnel_8017E734(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(&D_shelter_b1_pod_access_tunnel_80182D2C, 2U), arg0, 0, 0x100);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_3", func_shelter_b1_pod_access_tunnel_8017E778);
