#include "common.h"
#include "main/fs.h"
#include "main/task.h"
extern TaskDesc D_shelter_b3_dumping_hole_80188C04;

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017D9A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017DA00);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017DCFC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017DF90);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017E440);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017E7DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017E94C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017EDB8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017F1B0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017F820);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FB70);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FBA0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FCA0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FCF4);

void func_shelter_b3_dumping_hole_8017FD9C(s32 arg0, s32 arg1)
{
    if ((arg1 << 0x10) == 0) {
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, 0, arg0);
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, -0xA, arg0);
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, 0xA, arg0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FE10);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FE34);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FE64);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FE9C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FED4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FEF4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FF14);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FFF4);

void func_shelter_b3_dumping_hole_80180014(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_shelter_b3_dumping_hole_80180034(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8018005C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8018098C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_80181430);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_80181560);
