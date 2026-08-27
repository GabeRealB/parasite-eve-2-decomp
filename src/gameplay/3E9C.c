#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"

extern s32     Gp_LcgState;
extern SVECTOR D_801124DC[];
extern SVECTOR D_801125EC[];

void func_800F1BEC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_800F68AC(VECTOR3* arg0, s32 arg1, s32 arg2);

void func_800ECAA8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    s32            temp;
    s32            idx;
    s32            t2;
    s32            rng;
    s32            count;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    base  = Gp_RoomCoords;
    slot  = (GpCoordTail*)&base->coord;
    if (Gp_State1C->field_4 < 2) {
        mem->field_22++;
        switch (arg0->state) {
            case 0:
                temp                   = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->field_20          = temp;
                arg0->spawnArg1        = (u8)arg0->spawnArg1;
                slot->coord.coord.t[0] = coord->coord.t[0];
                slot->coord.coord.t[1] = coord->coord.t[1];
                t2                     = coord->coord.t[2];
                base->coord.flg        = 0;
                slot->field_50         = 0xC00;
                slot->field_52         = 0xC00;
                slot->field_54         = 0xC00;
                slot->field_58         = 0xFA0;
                slot->field_5C         = 0x12C0;
                slot->coord.coord.t[2] = t2;
                coord->sub             = mem->field_8;
                coord->coord.t[0]      = D_801124DC[arg0->spawnArg1].vx;
                coord->coord.t[1]      = D_801124DC[arg0->spawnArg1].vy;
                coord->coord.t[2]      = D_801124DC[arg0->spawnArg1].vz;
                coord->flg             = 0;
                Gp_UpdateCoord(coord);
                switch (arg0->spawnArg1) {
                    case 1:
                    default:
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rng;
                        func_800EA478(0x60034, coord, (((u32)rng >> 16) & 0x1FF) | 0x200, 0);
                        idx         = arg0->spawnArg1;
                        arg0->state = 1;
                        func_800EA478(0x60036, coord, idx, &D_801125EC[idx]);
                        mem->field_24 = 4;
                        base->field_0 = 4;
                        break;
                    case 2:
                    case 3:
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rng;
                        func_800EA478(0x60034, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx         = arg0->spawnArg1;
                        arg0->state = 1;
                        func_800EA478(0x60036, coord, idx, &D_801125EC[idx]);
                        mem->field_24 = 4;
                        base->field_0 = 4;
                        break;
                    case 30:
                    case 31:
                    case 32:
                        arg0->state = 2;
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rng;
                        func_800EA478(0x60034, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx = arg0->spawnArg1;
                        func_800EA478(0x60036, coord, idx, &D_801125EC[idx]);
                        mem->field_24 = 2;
                        base->field_0 = 2;
                        break;
                    case 5:
                        idx         = arg0->spawnArg1;
                        arg0->state = 1;
                        func_800EA478(0x60066, coord, idx, &D_801125EC[idx]);
                        mem->field_24 = 4;
                        base->field_0 = 0;
                        break;
                    case 33:
                        mem->field_20 = 1;
                        arg0->state   = 1;
                        rng           = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState   = rng;
                        func_800EA478(0x60034, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx = arg0->spawnArg1;
                        func_800EA478(0x60066, coord, idx, &D_801125EC[idx]);
                        mem->field_24 = 4;
                        base->field_0 = 0;
                        break;
                }
                if (mem->field_20 == 0) {
                    Gp_State1C->field_14 = 1;
                }
                break;
            case 1:
                rng         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rng;
                func_800EA478(0x60035, coord, (((u32)rng >> 16) & 0x1FF) | 0x200, 0);
                arg0->state++;
                break;
        }
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        count = mem->field_22;
        if (mem->field_24 < count) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void func_800ECEC0(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpState1C*     st;
    s32            t2;

    base  = Gp_RoomCoords;
    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    slot  = (GpCoordTail*)&base->coord;
    if (Gp_State1C->field_4 < 2) {
        mem->field_22++;
        switch (arg0->state) {
            case 0:
                slot->coord.coord.t[0] = coord->coord.t[0];
                slot->coord.coord.t[1] = coord->coord.t[1];
                t2                     = coord->coord.t[2];
                base->coord.flg        = 0;
                slot->field_54         = 0xC00;
                slot->field_52         = 0xC00;
                slot->field_50         = 0xC00;
                slot->field_58         = 0xFA0;
                slot->field_5C         = 0x12C0;
                Gp_RoomCoords->field_0 = 4;
                slot->coord.coord.t[2] = t2;
                coord->sub             = mem->field_8;
                coord->coord.t[0]      = D_801124DC[arg0->spawnArg1].vx;
                coord->coord.t[1]      = D_801124DC[arg0->spawnArg1].vy;
                coord->coord.t[2]      = D_801124DC[arg0->spawnArg1].vz;
                coord->flg             = 0;
                Gp_UpdateCoord(coord);
                mem->field_10 = 0;
                mem->field_12 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = ((u32)Gp_LcgState >> 16) & 0x1FF;
                {
                    s32 sh;
                    sh = mem->field_24;
                    __asm__("" : "+r"(sh));
                    mem->field_14 = -((s16)sh >> 1);
                }
                func_800EA478(0x60034, coord, mem->field_24 + 0x600, (SVECTOR*)&mem->field_10);
                st           = Gp_State1C;
                arg0->state  = 1;
                st->field_14 = 1;
                break;
            case 1:
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                func_800EA478(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x11280,
                              (s32)&mem->field_10);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                func_800EA478(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x21280,
                              (s32)&mem->field_10);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                func_800EA478(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x31280,
                              (s32)&mem->field_10);
                arg0->state++;
                break;
        }
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        if (mem->field_22 >= 5) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void func_800ED198(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpState1C*     st;
    s32            temp;
    s32            idx;
    s32            t2;
    s32            count;

    base  = Gp_RoomCoords;
    slot  = (GpCoordTail*)&base->coord;
    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    st    = Gp_State1C;
    if (st->field_4 < 2) {
        mem->field_22++;
        if (arg0->state == 0) {
            temp                   = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->field_20          = temp;
            arg0->spawnArg1        = (u8)arg0->spawnArg1;
            slot->coord.coord.t[0] = coord->coord.t[0];
            slot->coord.coord.t[1] = coord->coord.t[1];
            t2                     = coord->coord.t[2];
            base->coord.flg        = 0;
            slot->field_50         = 0xC00;
            slot->field_52         = 0xC00;
            slot->field_54         = 0xC00;
            slot->field_58         = 0xFA0;
            slot->field_5C         = 0x12C0;
            slot->coord.coord.t[2] = t2;
            coord->sub             = mem->field_8;
            coord->coord.t[0]      = D_801124DC[arg0->spawnArg1].vx;
            coord->coord.t[1]      = D_801124DC[arg0->spawnArg1].vy;
            coord->coord.t[2]      = D_801124DC[arg0->spawnArg1].vz;
            coord->flg             = 0;
            Gp_UpdateCoord(coord);
            mem->field_10 = 0;
            mem->field_12 = 0;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = ((u32)Gp_LcgState >> 16) & 0x1FF;
            {
                s32 sh;
                sh = mem->field_24;
                __asm__("" : "+r"(sh));
                mem->field_14 = -((s16)sh >> 1);
            }
            func_800EA478(0x60034, coord, mem->field_24 + 0x380, (SVECTOR*)&mem->field_10);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            func_800EA478(0x60072, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x380, 0);
            idx         = arg0->spawnArg1;
            arg0->state = 1;
            func_800EA478(0x60067, coord, idx, &D_801125EC[idx]);
            if (arg0->spawnArg1 == 0x11) {
                mem->field_24          = 1;
                Gp_RoomCoords->field_0 = 1;
            } else {
                mem->field_24          = 4;
                Gp_RoomCoords->field_0 = 4;
            }
            if (mem->field_20 == 0) {
                Gp_State1C->field_14 = 1;
            }
        }
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        count = mem->field_22;
        if (mem->field_24 < count) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ED42C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EDDFC);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EE210);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EE72C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EEC14);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EF0E0);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EF4D0);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EFBC4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F02B4);

void func_800F1364(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            rng;

    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    mem   = arg0->spawnArg2;
    if (arg0->state == 0) {
        if (arg0->spawnArg1 & 0xF0000) {
            mem->field_24 = (u32)arg0->spawnArg1 >> 16;
        } else {
            mem->field_24 = 0xC;
        }
        arg0->spawnArg1    = (u16)arg0->spawnArg1;
        coord->sub         = mem->field_8;
        coord->coord.t[0]  = D_801125EC[arg0->spawnArg1].vx;
        coord->coord.t[1]  = D_801125EC[arg0->spawnArg1].vy;
        coord->coord.t[2]  = D_801125EC[arg0->spawnArg1].vz;
        coord->coord.t[0] += mem->field_18;
        coord->coord.t[1] += mem->field_1A;
        coord->coord.t[2] += mem->field_1C;
        coord->flg         = 0;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        func_800EA478(0x60035, coord, (((u32)rng >> 16) & 0x1FF) | 0x11200, 0);
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        func_800EA478(0x60035, coord, (((u32)rng >> 16) & 0x1FF) | 0x21200, 0);
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        func_800EA478(0x60035, coord, (((u32)rng >> 16) & 0x1FF) | 0x31200, 0);
    }
    func_800EA478(0x60091, coord, arg0->spawnArg1, 0);
    mem->field_22++;
    if (mem->field_22 > mem->field_24 - 1) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

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
    Gp_UpdateCoord(coord);

    for (; i < 6; i++) {
        func_800EA478(0x60036, coord, 9, 0);
    }

    Gp_ReleaseState1CMem(mem, arg0);
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
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            rng           = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState   = rng;
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
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 < 4) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1BEC);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1FF4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F289C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F3414);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F3A78);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F4308);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F4D24);

void func_800F5184(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag < 4) {
        if (flag < 2) {
            if (arg0->state == 0) {
                mem->field_24 = 0x10;
                mem->field_26 = 0x20;
                mem->field_28 = D_8011291C[arg0->spawnArg1].field_0;
                mem->field_2A = D_8011291C[arg0->spawnArg1].field_2;
                arg0->state++;
            }
            Gp_UpdateCoord(coord);
            mem->field_24 -= 2;
            mem->field_26 += mem->field_2A;
            func_800F52B4(coord, mem->field_26, mem->field_24, mem->field_28);
            mem->field_22++;
            if (mem->field_22 < 8) {
                return;
            }
        } else {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

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
            Gp_UpdateCoord(coord);
            arg0->state = 1;
        } else if (Gp_State1C->field_8 >= 0) {
            if (!(((GameActorExt*)slot->extra)->field_C & 0x80)) {
                Gp_UpdateCoord(coord);
                if ((s16)func_800EA1A8((VECTOR3*)coord->workm.t, &vec) != 0) {
                    func_800F68AC(&vec, 0x1C0, Gp_State1C->field_8);
                }
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F6D18);
