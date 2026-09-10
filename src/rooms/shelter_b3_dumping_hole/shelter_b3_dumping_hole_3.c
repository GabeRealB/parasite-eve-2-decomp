#include "common.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
extern TaskDesc D_shelter_b3_dumping_hole_80188C04;
extern TaskDesc D_shelter_b3_dumping_hole_80188BC8;
extern s16      D_shelter_b3_dumping_hole_8018809C;

typedef struct {
    u8    pad_00[0x28];
    Task* field_28;
    Task* field_2C;
    s16   field_30;
    s16   field_32;
    u8    pad_34[0x4];
    s16   field_38;
    s16   field_3A;
    u8    pad_3C[0xC];
    s16   field_48;
    u8    pad_4A[0x2];
    u16   field_4C;
} DumpingHoleEntity;

typedef struct {
    u8  pad_0[0x2];
    s16 r;
    s16 g;
    s16 b;
} DumpingHoleFadeWork;

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

void func_shelter_b3_dumping_hole_8017FBA0(Task* arg0)
{
    DumpingHoleFadeWork* fade;
    DumpingHoleFadeWork* alloc;
    DumpingHoleEntity*   ent;

    ent  = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    fade = (DumpingHoleFadeWork*)arg0->idMap;
    if (ent->field_4C == 1) {
        Task_Kill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc       = (DumpingHoleFadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
                Task_Kill(arg0);
            }
            break;
    }
}

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

void func_shelter_b3_dumping_hole_8017FE9C(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    Gp_DispatchMsg(p->field_2C, 0x7D5, arg0, 0);
}

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
