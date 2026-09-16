#include "common.h"

#include "main/gameflag.h"
#include "gameplay/3CD8.h"

s32 func_neo_ark_substation_8017D768(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0xDF) != 0 ? 3 : 5);
    }
    return 0;
}

s32 func_neo_ark_substation_8017D7A4(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_substation/neo_ark_substation_2", func_neo_ark_substation_8017D7AC);

void func_neo_ark_substation_8017D814(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_substation/neo_ark_substation_2", jtbl_neo_ark_substation_8017D5EC);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_substation/neo_ark_substation_2", D_neo_ark_substation_8017D608);
