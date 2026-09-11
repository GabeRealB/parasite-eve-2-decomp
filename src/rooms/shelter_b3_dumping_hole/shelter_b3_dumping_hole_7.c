#include "common.h"
#include <psyq/libgte.h>
#include "gameplay/D4.h"
#include "main/task.h"

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
} DumpingHoleTarget;

typedef struct {
    DumpingHoleTarget* field_0;
    DumpingHoleTarget* field_4;
    s16                field_8;
    s16                field_A;
} DumpingHoleEntity;

typedef struct {
    u8                 _pad0[0x1C];
    DumpingHoleEntity* field_1C;
    u8                 _pad20[0x10];
    s32                field_30;
    s16                field_34;
    s16                field_36;
} DumpingHoleState;

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
extern TaskDesc        D_shelter_b3_dumping_hole_8018B83C;

void func_shelter_b3_dumping_hole_80183F04(DumpingHoleState* arg0);

extern s32     D_shelter_b3_dumping_hole_8018F4D8;
extern SVECTOR D_shelter_b3_dumping_hole_8018B86C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B8BC[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B92C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B93C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B94C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B95C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B96C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B974[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B98C[];
void           Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void           Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);

void func_shelter_b3_dumping_hole_80183CA0(DumpingHoleState* arg0)
{
    DumpingHoleDispatchDesc desc;
    DumpingHoleEntity*      ent = arg0->field_1C;
    DumpingHoleTarget*      t0  = ent->field_0;

    if (t0 != NULL) {
        DumpingHoleTarget2* t00 = t0->field_0;
        DumpingHoleP2C*     p   = t00->field_2C;
        p->field_24             = 3;
        p->field_25             = 5;
        t0->field_A             = 0x900;
        desc.field_0            = 0;
        desc.field_1            = 0x2E;
        desc.field_2            = arg0->field_34;
        Gp_DispatchMsg((Task*)t00, 0x7DB, (s32)&desc, 0);
    }
    ent->field_8    = 0;
    arg0->field_30 += 1;
}

void func_shelter_b3_dumping_hole_80183D34(DumpingHoleState* arg0)
{
    DumpingHoleEntity* ent = arg0->field_1C;
    DumpingHoleTarget* t   = ent->field_4;

    func_shelter_b3_dumping_hole_80183F04(arg0);
    if (ent->field_4 != NULL) {
        if ((s16)(ent->field_8 += 1) < 0x3D) {
            return;
        }
        {
            DumpingHoleTarget2*     t00 = ent->field_4->field_0;
            DumpingHoleP2C*         p   = t00->field_2C;
            DumpingHoleDispatchDesc desc;
            p->field_24  = 3;
            p->field_25  = 5;
            t->field_A   = 0x900;
            desc.field_0 = 0;
            desc.field_1 = 0x2E;
            desc.field_2 = arg0->field_34;
            Gp_DispatchMsg((Task*)t00, 0x7DB, (s32)&desc, 0);
        }
    }
    ent->field_8   = 0;
    arg0->field_30 += 1;
}

void func_shelter_b3_dumping_hole_80183E08(DumpingHoleState* arg0)
{
    DumpingHoleEntity* ent = arg0->field_1C;
    func_shelter_b3_dumping_hole_80183F04(arg0);
    if (ent->field_A == 3) {
        D_shelter_b3_dumping_hole_8018B7BC[arg0->field_36].field_6 = 2;
        Task_Kill((Task*)arg0);
    }
}

void func_shelter_b3_dumping_hole_80183E6C(s16 arg0, s16 arg1, s16 arg2)
{
    switch (arg1) {
        case 0:
            Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 1, (arg0 << 16) + arg2, 0);
            break;
        case 1:
            Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 2, (arg0 << 16) + arg2, 0);
            break;
        case 2:
            Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 3, (arg0 << 16) + arg2, 0);
            break;
    }
}

typedef struct {
    u8  _pad0[0x40];
    s16 field_40;
} DumpingHolePairTarget;

typedef struct {
    DumpingHolePairTarget* field_0;
    DumpingHolePairTarget* field_4;
    u8                     _pad8[0x2];
    u16                    field_A;
} DumpingHolePairC;

void func_shelter_b3_dumping_hole_80183F04(DumpingHoleState* arg0)
{
    DumpingHolePairC* p = (DumpingHolePairC*)arg0->field_1C;

    if (p->field_0 != NULL) {
        if (p->field_0->field_40 <= 0) {
            p->field_0 = NULL;
        }
    } else {
        p->field_A |= 1;
    }
    if (p->field_4 != NULL) {
        if (p->field_4->field_40 <= 0) {
            p->field_4 = NULL;
        }
    } else {
        p->field_A |= 2;
    }
}

void func_shelter_b3_dumping_hole_80183F84(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_shelter_b3_dumping_hole_8018F4D8 = 0;
        task->state                        = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x100);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[4], 0x300, 0x400);
            break;
        case 3:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x400);
            break;
        case 4:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[9], 0x300, 0x400);
            break;
        case 7:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[0], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[1], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[2], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[3], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[4], 0x300, 0x400);
            break;
        case 14:
            if (D_shelter_b3_dumping_hole_8018F4D8 != 0) {
                Room_Draw13(&D_shelter_b3_dumping_hole_8018B95C[0], 0x280, 0x44);
                Room_Draw13(&D_shelter_b3_dumping_hole_8018B95C[1], 0x280, 0x40);
            }
            break;
        case 15:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x100);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x21], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x22], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x23], 0x300, 0x400);
            break;
        case 17:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B98C[0], 0x300, 0x400);
            break;
        case 18:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B8BC[0], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x12], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x13], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x19], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x1A], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x1B], 0x300, 0x200);
            break;
        case 19:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x400);
            break;
        case 21:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B92C[0], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B92C[1], 0x400, 0x444);
            break;
        case 22:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B95C[0], 0x280, 0x44);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B95C[1], 0x280, 0x40);
            break;
        case 23:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x400);
            break;
        case 26:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B93C[0], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B93C[1], 0x300, 0x400);
            break;
        case 29:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[2], 0x280, 0x44);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[3], 0x280, 0x40);
            break;
        case 30:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[2], 0x280, 0x44);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[3], 0x280, 0x40);
            break;
        case 31:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x400);
            break;
        case 34:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[4], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[5], 0x300, 0x400);
            break;
        case 35:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[6], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[7], 0x300, 0x400);
            break;
        case 13:
        case 37:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[6], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[16], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[25], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[27], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x100);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[35], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x100);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x300);
            break;
    }
}
