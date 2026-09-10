#include "common.h"
#include "main/task.h"
#include "main/session.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"

typedef struct {
    u8 _pad0[0x24];
    u8 field_24;
    u8 field_25;
} DumpingHoleP2C;

typedef struct {
    u8              _pad0[0x2C];
    DumpingHoleP2C* field_2C;
} DumpingHoleTarget2;

typedef struct {
    DumpingHoleTarget2* field_0;
    u8                  _pad4[0x6];
    s16                 field_A;
    u8                  _pad0C[0x34];
    s16                 field_40;
} DumpingHoleTarget;

typedef struct {
    DumpingHoleTarget* field_0;
    s16                field_4;
} DumpingHoleEntity;

typedef struct {
    DumpingHoleTarget* field_0;
    u16                field_4;
} DumpingHoleEntityB;

typedef struct {
    u8                 _pad0[0x1C];
    DumpingHoleEntity* field_1C;
    u8                 _pad20[0x10];
    s32                field_30;
    s16                field_34;
    s16                field_36;
} DumpingHoleState;

typedef struct {
    u8  _pad0[0x2];
    u16 field_2;
} DumpingHoleMsg;

typedef struct {
    u8  _pad0[0x6];
    s16 field_6;
} DumpingHoleB7BC;

typedef struct {
    u8  field_0;
    u8  field_1;
    u16 field_2;
} DumpingHoleDispatchDesc;

extern DumpingHoleB7BC D_shelter_b3_dumping_hole_8018B7BC[];

void func_shelter_b3_dumping_hole_801833EC(DumpingHoleState* arg0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183144);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183198);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183218);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183298);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_801833EC);

void func_shelter_b3_dumping_hole_80183530(DumpingHoleState* arg0, s32 arg1, DumpingHoleMsg* arg2)
{
    DumpingHoleEntity* ent = arg0->field_1C;
    if (arg2->field_2 == 4) {
        ent->field_4 = arg2->field_2;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183550);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_801835C8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183620);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183678);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_801836E0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_8018378C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183824);

void func_shelter_b3_dumping_hole_801838A0(DumpingHoleState* arg0)
{
    s16 count;
    s32 i;

    count = 0;
    if (arg0->field_1C->field_4 != 4) {
        func_shelter_b3_dumping_hole_801833EC(arg0);
        for (i = 0; i < 0x10; i++) {
            if (D_shelter_b3_dumping_hole_8018B7BC[i].field_6 == 2) {
                count++;
            }
        }
        if (count == 0x10) {
            ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)((Task*)arg0, 0);
            Game_Session->unknown_130[0] = 2;
            Task_Kill((Task*)arg0);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183950);

void func_shelter_b3_dumping_hole_80183A00(DumpingHoleState* arg0)
{
    DumpingHoleDispatchDesc desc;
    DumpingHoleEntityB*     ent = (DumpingHoleEntityB*)arg0->field_1C;
    DumpingHoleTarget*      t0  = ent->field_0;
    DumpingHoleTarget2*     t00 = t0->field_0;

    if ((s16)(ent->field_4 += 1) >= 0x2E) {
        DumpingHoleP2C* p = t00->field_2C;
        p->field_25       = 2;
        p->field_24       = 0;
        t0->field_A       = 0x900;
        desc.field_0      = 0;
        desc.field_1      = 0x2C;
        desc.field_2      = arg0->field_34;
        Gp_DispatchMsg((Task*)t00, 0x7DB, (s32)&desc, 0);
        arg0->field_30 += 1;
    }
}

void func_shelter_b3_dumping_hole_80183A98(DumpingHoleState* arg0)
{
    if (arg0->field_1C->field_0->field_40 <= 0) {
        D_shelter_b3_dumping_hole_8018B7BC[arg0->field_36].field_6 = 2;
        Task_Kill((Task*)arg0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_6", func_shelter_b3_dumping_hole_80183AEC);

void func_shelter_b3_dumping_hole_80183B9C(DumpingHoleState* arg0)
{
    DumpingHoleDispatchDesc desc;
    DumpingHoleEntityB*     ent = (DumpingHoleEntityB*)arg0->field_1C;
    DumpingHoleTarget*      t0  = ent->field_0;
    DumpingHoleTarget2*     t00 = t0->field_0;

    if ((s16)(ent->field_4 += 1) >= 0x3D) {
        DumpingHoleP2C* p = t00->field_2C;
        p->field_24       = 2;
        p->field_25       = 4;
        t0->field_A       = 0x900;
        desc.field_0      = 0;
        desc.field_1      = 0x2A;
        desc.field_2      = arg0->field_34;
        Gp_DispatchMsg((Task*)t00, 0x7DB, (s32)&desc, 0);
        arg0->field_30 += 1;
    }
}

void func_shelter_b3_dumping_hole_80183C38(DumpingHoleState* arg0)
{
    if (arg0->field_1C->field_0->field_40 <= 0) {
        D_shelter_b3_dumping_hole_8018B7BC[arg0->field_36].field_6 = 2;
        Task_Kill((Task*)arg0);
    }
}
