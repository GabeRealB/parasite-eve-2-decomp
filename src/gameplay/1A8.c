#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1A8.h"
#include "main/session.h"
#include "main/task.h"

void func_800E1B80(void);
void func_800AD6BC(void);
s32  func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_8003D5D8(SVECTOR* v0, SVECTOR* v1);
s32  func_8003D9C8(s32 y, s32 x);

extern u16 D_80114CD2;
extern u16 D_80114CD4;
extern u16 D_80114CD6;
extern u8  D_80114CD8;
extern u8  D_80114CD9;
extern u8  D_80114CDA;
extern u8  D_80114CDB;
extern u8  D_80114CF8;

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AEBA4);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AED24);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AED80);

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
    func_8003D5D8(&vec, &vec);
    return func_8003D9C8(vec.vx, vec.vz) & 0xFFF;
}

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AEE8C);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AEEFC);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AEF4C);

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

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF180);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF208);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF284);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF314);

void func_800AF3D0(void)
{
    if (func_800AC464(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        D_80114CD6++;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF41C);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF498);

INCLUDE_ASM("gameplay/nonmatchings/1A8", func_800AF500);
