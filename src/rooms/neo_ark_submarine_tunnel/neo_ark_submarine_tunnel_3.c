#include "common.h"
#include "main/task.h"
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

void func_neo_ark_submarine_tunnel_8017F48C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x6027F;
        D_80115730  = 0x60280;
        D_80115754  = 0x60281;
        arg0->state = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_3", func_neo_ark_submarine_tunnel_8017F4DC);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_3", func_neo_ark_submarine_tunnel_8017FA34);
