#include "common.h"

extern u8   D_8007216C;
extern u8   D_shelter_b4_upper_sewer_80188D2C;
extern void func_shelter_b4_upper_sewer_8017E59C(s32);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", D_shelter_b4_upper_sewer_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", D_shelter_b4_upper_sewer_8017D5C4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017D660);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017D80C);

s32 func_shelter_b4_upper_sewer_8017D9BC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017D9C4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017DAB0);

s32 func_shelter_b4_upper_sewer_8017DB50(void)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB58(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        func_shelter_b4_upper_sewer_8017E59C(1);
        SndEvt_EnqueueType6(0x542C0004, 0, 0);
    }
    return 0;
}

void func_shelter_b4_upper_sewer_8017DB94(void)
{
    D_8007216C = D_shelter_b4_upper_sewer_80188D2C;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017DBA8);

void func_shelter_b4_upper_sewer_8017DC28(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017DC30);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017DC88);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017DD98);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017E4F4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", jtbl_shelter_b4_upper_sewer_8017D604);
