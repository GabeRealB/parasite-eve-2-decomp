#include "common.h"

extern s16 D_800691CA;

s32 func_neo_ark_r31_8017D8FC(void)
{
    return 0;
}

s32 func_neo_ark_r31_8017D904(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_r31/neo_ark_r31_2", func_neo_ark_r31_8017D90C);

void func_neo_ark_r31_8017D980(void)
{
    D_800691CA = 2;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_r31/neo_ark_r31_2", func_neo_ark_r31_8017D990);
