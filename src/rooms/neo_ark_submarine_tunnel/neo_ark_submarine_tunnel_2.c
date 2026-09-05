#include "common.h"
#include "main/task.h"
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_2", func_neo_ark_submarine_tunnel_8017F2C8);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_2", func_neo_ark_submarine_tunnel_8017F318);

void func_neo_ark_submarine_tunnel_8017F398(s32 arg0)
{
    GameFlag_SetNibble(0xBC, arg0);
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_2", func_neo_ark_submarine_tunnel_8017F3BC);

void func_neo_ark_submarine_tunnel_8017F414(void)
{
    Game_GetPtrSlot(3);
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_2", jtbl_neo_ark_submarine_tunnel_8017D620);
