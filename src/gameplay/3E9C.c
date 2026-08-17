#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_80070F60;

void func_800F1BEC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_800F68AC(VECTOR3* arg0, s32 arg1, s32 arg2);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ECAA8);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ECEC0);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ED198);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ED42C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EDDFC);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EE210);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EE72C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EEC14);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EF0E0);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EF4D0);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EFBC4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F02B4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1364);

void func_800F1594(Task* arg0)
{
    GsCOORDINATE2* coord;
    MATRIX*        m;
    void*          mem;
    s32            i;
    s32            one;

    i                    = 0;
    one                  = ONE;
    coord                = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    mem                  = arg0->spawnArg2;
    m                    = &coord->coord;
    *(s32*)&coord->coord = one;
    *(s32*)&m->m[0][2]   = 0;
    *(s32*)&m->m[1][1]   = one;
    *(s32*)&m->m[2][0]   = 0;
    m->m[2][2]           = one;
    coord->flg           = 0;
    func_80098F58(coord);

    for (; i < 6; i++) {
        func_800EA478(0x60036, coord, 9, 0);
    }

    func_800EC7E4(mem, arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1638);

void func_800F1A9C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            i;
    s32            rng;

    mem   = arg0->spawnArg2;
    flag  = D_80115740->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag < 2) {
        func_80098F58(coord);
        if (arg0->state == 0) {
            rng           = D_80070F60 * 5 + 0x71357911;
            mem->field_24 = ((u32)rng >> 16) & 0xFFF;
            D_80070F60    = rng;
            if (arg0->spawnArg1 & 0xFFF) {
                mem->field_26 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            } else {
                mem->field_26 = 0x200;
            }
            for (i = 0; i < 6; i++) {
                func_800EA478(0x600A4, coord, 1, 0);
            }
            arg0->state = 1;
        }
        func_800F1BEC(coord, mem->field_22, mem->field_26, mem->field_24);
        if (D_80115740->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 < 4) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    func_800EC7E4(mem, arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1BEC);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1FF4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F289C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F3414);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F3A78);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F4308);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F4D24);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F5184);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F52B4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F59DC);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F5E1C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F6560);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F68AC);

void func_800F6C2C(Task* arg0)
{
    VECTOR3        vec;
    Task*          slot;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;

    slot  = Game_GetPtrSlot(3);
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (slot != NULL) {
        if (arg0->state == 0) {
            parent     = (GsCOORDINATE2*)((GameActorExt*)slot->extra)->field_8;
            coord->flg = 0;
            coord->sub = parent + 1;
            func_80098F58(coord);
            arg0->state = 1;
        } else if (D_80115740->field_8 >= 0) {
            if (!(((GameActorExt*)slot->extra)->field_C & 0x80)) {
                func_80098F58(coord);
                if (func_800EA1A8((VECTOR3*)coord->workm.t, &vec) != 0) {
                    func_800F68AC(&vec, 0x1C0, D_80115740->field_8);
                }
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F6D18);
