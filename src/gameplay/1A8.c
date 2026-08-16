#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

void func_800AD6BC(void);

extern TaskFuncTable3   D_80093950;
extern GpVoidFuncTable6 D_80093978;
extern u16              D_80114CD0;
extern u16              D_80114CD2;
extern u16              D_80114CD4;
extern u16              D_80114CD6;
extern u8               D_80114CD8;
extern u8               D_80114CD9;
extern u8               D_80114CDA;
extern u8               D_80114CDB;
extern u8               D_80114CDC;
extern u8               D_80114CDD;
extern u8               D_80114CDE;
extern s16              D_80114CE0;
extern u16              D_80114CF6;
extern u8               D_80114CF8;
extern s32              D_80114D00[2];
extern s16              D_80114D08;

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AEBA4);

void func_800AED24(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = D_8010CBCC[arg0->field_3];
    if (rec != NULL) {
        obj = rec[arg0->field_2].field_4;
        if (obj != NULL) {
            obj->field_1 &= 0xFB;
        }
    }
}

void func_800AED80(Task* arg0)
{
    D_80114CDE    = 0;
    D_80114CDD    = 0;
    D_80114CD2    = 0;
    D_80114CD0    = 0;
    D_80114CDC    = 0;
    D_80114CD8    = 0;
    D_80114CD9    = 0;
    D_80114CD6    = 0;
    D_80114CF8    = 0;
    D_80114CE0    = 1;
    D_80114D00[0] = 0;
    D_80114D00[1] = 0;
    D_80114D08    = 0xA;
    arg0->state++;
}

void func_800AEE00(void)
{
    func_800E1B80();
    func_800AD6BC();
}

s32 func_800AEE28(Task* arg0, GpPosXZ* arg1)
{
    SVECTOR    vec;
    GpCoordXZ* coord;

    coord  = (GpCoordXZ*)((GameActorExt*)arg0->extra)->field_8;
    vec.vx = arg1->vx - coord->field_18;
    vec.vy = 0;
    vec.vz = arg1->vz - coord->field_20;
    VectorNormalSS(&vec, &vec);
    return ratan2(vec.vx, vec.vz) & 0xFFF;
}

void func_800AEE8C(Task* arg0)
{
    TaskFuncTable3 sp;
    void*          slot;

    slot = Game_GetPtrSlot(3);
    sp   = D_80093950;
    if (slot != NULL) {
        sp.funcs[arg0->state](arg0);
    }
}

u8 func_800AEEFC(void)
{
    GameSession* session;
    GpCb40Tbl*   tbl;

    session = Game_Session;
    tbl     = D_8010CB40[session->field_7 - 1];
    return tbl->field_0[session->field_6 - 1][session->field_5 - 1].field_0;
}

void func_800AEF4C(void)
{
    GpVoidFuncTable6 sp;

    sp = D_80093978;
    sp.funcs[*(s16*)&D_80114CD6]();
}

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AEFBC);

void func_800AF070(void)
{
    D_80114CD9 = 0;
    D_80114CD8 = 0;
    D_80114CD2 = 0;
    D_80114CDB = 0;
    D_80114CDA = 0;
    D_80114CD4 = 0;
    D_80114CF8 = 0;
}

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF0AC);

void func_800AF180(void)
{
    if (Game_Session->field_1 == 0) {
        if (func_800E6CE0() == 0) {
            func_800E40BC(D_80114CD8, D_80114CD9);
        }
    }
    D_80114CF8 = 0;
    D_80114CD9 = 0;
    D_80114CD8 = 0;
    D_80114CD2 = 0;
    D_80114CDB = 0;
    D_80114CDA = 0;
    D_80114CD4 = 0;
}

void func_800AF208(void)
{
    u8 fade;

    if (*(s16*)&D_80114CF6 != 0) {
        fade = *(u8*)&D_80114CF6;
        Fade_DrawOverlay(fade, fade, fade, 2);
        D_80114CF6 += 0x1E;
        if ((s16)D_80114CF6 >= 0x100) {
            D_80114CF6 = 0xFF;
        }
    }
    D_80114CD6++;
}

void func_800AF284(void)
{
    u8 fade;

    if (*(s16*)&D_80114CF6 != 0) {
        fade = *(u8*)&D_80114CF6;
        Fade_DrawOverlay(fade, fade, fade, 2);
    }
    Mc_SaveData.field_6 = D_80114CE8.field_0;
    Mc_SaveData.field_8 = D_80114CE8.field_2;
    Mc_SaveData.field_5 = D_80114CE8.field_3;
    Task_Spawn(0, 0x11, 0, 0);
    D_80114CF8 = 0;
    D_80114CD9 = 0;
    D_80114CD8 = 0;
    D_80114CD2 = 0;
}

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF314);

void func_800AF3D0(void)
{
    if (func_800AC464(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        D_80114CD6++;
    }
}

void func_800AF41C(void)
{
    s32   sp[2];
    void* slot;

    slot  = Game_GetPtrSlot(3);
    sp[0] = (D_80114CD2 >> 8) & 1;
    sp[1] = D_80114CD8 & 0xF;
    func_800AC464(slot, 0x3EF, (s32)sp, 0);
    D_80114CDB = 0;
    D_80114CDA = 0;
    D_80114CD4 = 0;
    D_80114CD6++;
}

void func_800AF498(void)
{
    GpAreaKey* key;
    GpAreaRec* rec;
    GpAreaObj* obj;

    key = (GpAreaKey*)&Game_Session->field_4;
    rec = D_8010CBCC[key->field_3];
    if (rec != NULL) {
        obj = rec[key->field_2].field_4;
        if (obj != NULL) {
            obj->field_1 |= 0x4;
        }
    }
}

void func_800AF500(s16 arg0, GpAreaFlagRec* arg1)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = D_8010CBCC[arg0];
    if (rec != NULL) {
        for (; arg1->field_0 != 0xFF; arg1++) {
            if (arg1->field_1 != 0) {
                obj = rec[arg1->field_0].field_4;
                if (obj != NULL) {
                    obj->field_1 |= 0x4;
                }
            }
        }
    }
}
