#include "common.h"

s32 func_acropolis_promenade_8017D8E0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 5) {
        if (Gp_GetCurBit2Flag(0x15) != 2) {
            Gp_StartCapSlot(5, 1, 0);
        } else {
            Gp_RunCapCmd1(9);
        }
    }
    return 0;
}

void func_acropolis_promenade_8017D930(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017D938);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017D988);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017D9E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017DA4C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017DAA4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017DB48);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017DB9C);
