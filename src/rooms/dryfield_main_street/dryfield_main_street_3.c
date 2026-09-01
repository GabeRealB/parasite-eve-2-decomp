#include "common.h"
#include "main/task.h"

extern Task* D_dryfield_main_street_80185630;

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_3", func_dryfield_main_street_8017E168);

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_3", func_dryfield_main_street_8017E1C0);

void func_dryfield_main_street_8017E2F4(s32 arg0)
{
    Gp_ArmStateF0(arg0);
    Gp_SetItemSeenBit(0x10A, 1);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_3", func_dryfield_main_street_8017E320);

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_3", func_dryfield_main_street_8017E354);

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_3", func_dryfield_main_street_8017E3A8);

void func_dryfield_main_street_8017E4A4(void)
{
    D_dryfield_main_street_80185630 = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_3", func_dryfield_main_street_8017E4B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_3", func_dryfield_main_street_8017E830);
