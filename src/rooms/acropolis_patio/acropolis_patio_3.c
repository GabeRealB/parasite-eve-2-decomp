#include "common.h"

extern u8 D_801153F4;

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_3", func_acropolis_patio_8017DF8C);

void func_acropolis_patio_8017DFE4(s32 arg0)
{
    if (arg0 != 0) {
        D_801153F4 = 0;
        Gp_ArmStateF0(1);
        return;
    }
    D_801153F4 = 1;
}