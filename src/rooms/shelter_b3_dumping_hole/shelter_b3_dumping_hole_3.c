#include "common.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
extern TaskDesc D_shelter_b3_dumping_hole_80188C04;
extern TaskDesc D_shelter_b3_dumping_hole_80188BC8;
extern s16      D_shelter_b3_dumping_hole_8018809C;

typedef struct {
    u8    pad_00[0x28];
    Task* field_28;
    u8    pad_2C[0x4];
    s16   field_30;
    s16   field_32;
    u8    pad_34[0x4];
    s16   field_38;
    s16   field_3A;
    u8    pad_3C[0xC];
    s16   field_48;
} DumpingHoleEntity;

typedef struct {
    u8                 pad_00[0x1C];
    DumpingHoleEntity* field_1C;
} DumpingHoleState;

extern DumpingHoleState* D_shelter_b3_dumping_hole_8018F4A8;

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

s16 func_shelter_b3_dumping_hole_8017FB70(void)
{
    if (Game_Session->field_5 == 2) {
        return 0;
    }
    return D_shelter_b3_dumping_hole_8018809C;
}

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

void func_shelter_b3_dumping_hole_8017FE10(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    if (arg0 == 0) {
        p->field_48 = 1;
    }
}

void func_shelter_b3_dumping_hole_8017FE34(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188BC8, 1, 9, 0);
}

void func_shelter_b3_dumping_hole_8017FE64(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    Gp_DispatchMsg(p->field_28, 0x7D5, arg0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FE9C);

void func_shelter_b3_dumping_hole_8017FED4(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    p->field_30          = arg0;
    p->field_32          = 0;
}

void func_shelter_b3_dumping_hole_8017FEF4(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    p->field_38          = arg0;
    p->field_3A          = 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FF14);

void func_shelter_b3_dumping_hole_8017FFF4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D650);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D654);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D664);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D670);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D67C);
