#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

extern s32     Gp_LcgState;
extern SVECTOR D_801124DC[];
extern SVECTOR D_801125EC[];
extern SVECTOR D_801126FC[];
extern SVECTOR D_8011280C[];

void func_800EF0E0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800F1BEC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_800F68AC(VECTOR3* arg0, s32 arg1, s16 arg2);

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

void func_800EDDFC(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    s32            temp;
    s32            idx;
    s32            t2;
    s32            rng;
    s32            rng2;
    s32            count;
    s32            i;

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
                coord->coord.t[0]      = D_801126FC[arg0->spawnArg1].vx;
                coord->coord.t[1]      = D_801126FC[arg0->spawnArg1].vy;
                coord->coord.t[2]      = D_801126FC[arg0->spawnArg1].vz;
                coord->flg             = 0;
                Gp_UpdateCoord(coord);
                rng           = Gp_LcgState * 5 + 0x71357911;
                idx           = ((u32)rng >> 16) & 0x1FF;
                rng2          = rng * 5 + 0x71357911;
                Gp_LcgState   = rng;
                mem->field_24 = idx;
                Gp_LcgState   = rng2;
                func_800EF0E0(coord, idx | 0x400, ((u32)rng2 >> 16) & 0xFFF, idx);
                for (i = 0; i < 4; i++) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    func_800EA478(0x6006F, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x380, 0);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    func_800EA478(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x21380, 0);
                }
                switch (arg0->spawnArg1) {
                    case 1:
                    default:
                        arg0->state   = 1;
                        mem->field_24 = 0x10;
                        break;
                    case 12:
                        arg0->state   = 2;
                        mem->field_24 = 4;
                        break;
                    case 27:
                        mem->field_26 = 0xA;
                        arg0->state   = 1;
                        mem->field_24 = 0x18;
                        break;
                }
                base->field_0 = 4;
                if (mem->field_20 == 0) {
                    Gp_State1C->field_14 = 1;
                }
                break;
            case 1:
                if (mem->field_22 == mem->field_24) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    func_800EA478(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0xFF) + 0x12180,
                                  &D_8011280C[arg0->spawnArg1]);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    func_800EA478(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0xFF) + 0x22180,
                                  &D_8011280C[arg0->spawnArg1]);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    func_800EA478(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0xFF) + 0x32180,
                                  &D_8011280C[arg0->spawnArg1]);
                    func_800EA478(0x60091, coord, arg0->spawnArg1 + mem->field_26,
                                  &D_8011280C[arg0->spawnArg1]);
                    arg0->state = 2;
                }
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

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EE210);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EE72C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EEC14);

void func_800EF0E0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    register s32      u70 asm("a1");
    POLY_FT4*         prim;
    s32               ang;
    u16               vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                      = (GpEffBeamScratch*)(head - 0x1C);
    block->vec.vy                              = *(u16*)&arg0->workm.t[1];
    vz                                         = *(u16*)&arg0->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    vecp                                       = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        prim       = (POLY_FT4*)D_80071190;
        D_80071190 = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        u70         = 0x70;
        prim->u0    = u70;
        prim->u1    = 0xA7;
        prim->u2    = u70;
        prim->v2    = 0xFF;
        prim->u3    = 0xA7;
        prim->v3    = 0xFF;
        block->dx   = ((((s16)arg1 * 55) / block->otz) * rsin((s16)arg2)) >> 12;
        block->dy   = ((((s16)arg1 * 55) / block->otz) * rcos((s16)arg2)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        ang         = (s16)arg2 + 0x400;
        block->dx   = ((((s16)arg1 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy   = ((((s16)arg1 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

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

void func_800F1BEC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s32               uv;
    s32               ang;
    u16               vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                      = (GpEffBeamScratch*)(head - 0x1C);
    block->vec.vy                              = *(u16*)&arg0->workm.t[1];
    vz                                         = *(u16*)&arg0->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    vecp                                       = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        block->otz = block->otz + 1;
        prim       = (POLY_FT4*)D_80071190;
        D_80071190 = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x28;
        prim->clut  = 0x4241;
        uv          = arg1 * 0x18;
        prim->u0    = uv - 0x70;
        prim->u2    = uv - 0x70;
        prim->v0    = 0;
        prim->u1    = uv - 0x59;
        prim->v1    = 0;
        prim->v2    = 0x17;
        prim->u3    = uv - 0x59;
        prim->v3    = 0x17;
        block->dx   = ((((s16)arg2 * 23) / block->otz) * rsin((s16)arg3)) >> 12;
        block->dy   = ((((s16)arg2 * 23) / block->otz) * rcos((s16)arg3)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        ang         = (s16)arg3 + 0x400;
        block->dx   = ((((s16)arg2 * 23) / block->otz) * rsin(ang)) >> 12;
        block->dy   = ((((s16)arg2 * 23) / block->otz) * rcos(ang)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1FF4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F289C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F3414);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F3A78);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F4308);

void func_800F4D24(Task* arg0)
{
    GpEffLineScratch* block;
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    MATRIX*           m;
    LINE_F2*          prim;
    s16               step;
    s32               rng;
    s32               one;
    s16               val;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - 0x20;
    coord                   = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    block                   = (GpEffLineScratch*)*(void**)G_SCRATCH_HEAD;
    mem                     = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    if (mem->field_22 == 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_10 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_12 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_14 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        step          = 2;
        if ((((u32)Gp_LcgState >> 16) & 3) != 0) {
            step = 1;
        }
        rng                  = Gp_LcgState * 5 + 0x71357911;
        one                  = ONE;
        m                    = &coord->coord;
        mem->field_26        = (((u32)rng >> 16) & 1) + 1;
        mem->field_24        = step;
        *(s32*)&coord->coord = one;
        *(s32*)&m->m[0][2]   = 0;
        *(s32*)&m->m[1][1]   = one;
        *(s32*)&m->m[2][0]   = 0;
        m->m[2][2]           = one;
        mem->field_18        = *(u16*)&coord->workm.t[0];
        mem->field_1A        = *(u16*)&coord->workm.t[1];
        mem->field_1C        = *(u16*)&coord->workm.t[2];
        Gp_LcgState          = rng;
    }
    coord->coord.t[0] += mem->field_10;
    coord->coord.t[1] += mem->field_12;
    coord->coord.t[2] += mem->field_14;
    coord->flg         = 0;
    block->vec0.vx     = *(u16*)&coord->workm.t[0];
    block->vec0.vy     = *(u16*)&coord->workm.t[1];
    block->vec0.vz     = *(u16*)&coord->workm.t[2];
    block->vec1.vx     = *(u16*)&mem->field_18;
    block->vec1.vy     = *(u16*)&mem->field_1A;
    block->vec1.vz     = *(u16*)&mem->field_1C;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec0);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_ldv0(&block->vec1);
        gte_rtps_real();
        gte_stsxy(&block->sxy1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim       = (LINE_F2*)D_80071190;
            block->otz = block->otz + 1;
            D_80071190 = (DR_TPAGE*)(prim + 1);
            setlen(prim, 3);
            setcode(prim, 0x40);
            val = 0xFF - (mem->field_22 << (6 - mem->field_24));
            if (arg0->spawnArg1 != 0) {
                prim->r0 = val >> 3;
                prim->g0 = val >> mem->field_26;
                prim->b0 = val;
            } else {
                prim->r0 = val;
                prim->g0 = val >> mem->field_26;
                prim->b0 = val >> 3;
            }
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            mem->field_18 = *(u16*)&coord->workm.t[0];
            mem->field_1A = *(u16*)&coord->workm.t[1];
            mem->field_1C = *(u16*)&coord->workm.t[2];
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
    mem->field_22++;
    if (mem->field_22 > mem->field_24 * 8 - 1) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

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

void func_800F59DC(Task* arg0)
{
    register void**         scratch asm("a0");
    register u8*            head asm("v0");
    register GpQuadScratch* block asm("t1");
    register SVECTOR*       v asm("a1");
    register s32            i asm("a3");
    GpEffWork*              mem;
    GsCOORDINATE2*          coord;
    GpQuadCorner*           tbl;
    MATRIX*                 m;
    POLY_FT4*               prim;
    s32                     scale;
    s32                     shade;
    u8                      col;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (arg0->state == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
        coord->flg = 0;
        if (arg0->spawnArg1 & 0xFFF) {
            scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        } else {
            scale = 0x400;
        }
        mem->field_24 = scale;
        mem->field_26 = 0x3FF;
        arg0->state   = 1;
    }
    Gp_UpdateCoord(coord);

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x38;
    block    = (GpQuadScratch*)head;
    *scratch = block;
    i        = 0;
    m        = &coord->workm;
    v        = block->vec;
    tbl      = D_80111E38;
    do {
        v->vx = tbl->x * *(u16*)&mem->field_24;
        v->vy = 0;
        v->vz = tbl->y * *(u16*)&mem->field_24;
        gte_SetRotMatrix(m);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&coord->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&coord->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&coord->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    shade = -0x80 - (mem->field_22 >> 3);
    col   = shade;
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz += 0x80;
        prim        = (POLY_FT4*)D_80071190;
        D_80071190  = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->g0    = col >> 2;
        prim->b0    = col >> 2;
        prim->tpage = 0x29;
        prim->r0    = shade;
        prim->clut  = 0x428C;
        prim->u0    = 0xA8;
        prim->v0    = 0xC8;
        prim->u1    = 0xDF;
        prim->v1    = 0xC8;
        prim->u2    = 0xA8;
        prim->v2    = 0xFF;
        prim->u3    = 0xDF;
        prim->v3    = 0xFF;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
    mem->field_22++;
    if (mem->field_26 < mem->field_22) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F5E1C);

void func_800F6560(GsCOORDINATE2* arg0, s32 arg1, u32 arg2)
{
    register void**         scratch asm("v0");
    register u8*            head asm("v1");
    register GpQuadScratch* block asm("t1");
    register SVECTOR*       v asm("a3");
    register s32            i asm("t2");
    GpQuadCorner*           tbl;
    POLY_FT4*               prim;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x38;
    block    = (GpQuadScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        v->vx = tbl->x * arg1;
        v->vy = 0;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&D_80070F34);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim       = (POLY_FT4*)D_80071190;
        D_80071190 = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->r0    = arg2 >> 1;
        prim->tpage = 0x29;
        prim->clut  = 0x430F;
        prim->u0    = 0xE0;
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        prim->u2    = 0xE0;
        prim->g0    = arg2;
        prim->b0    = arg2;
        prim->u1    = 0xFF;
        prim->v2    = 0xE7;
        prim->u3    = 0xFF;
        prim->v3    = 0xE7;
        setSemiTrans(prim, 1);
        prim->x0 = block->sxy0.vx;
        prim->y0 = block->sxy0.vy;
        prim->x1 = block->sxy1.vx;
        prim->y1 = block->sxy1.vy;
        prim->x2 = block->sxy2.vx;
        prim->y2 = block->sxy2.vy;
        prim->x3 = block->sxy3.vx;
        prim->y3 = block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

void func_800F68AC(VECTOR3* arg0, s32 arg1, s16 arg2)
{
    register void**         scratch asm("v0");
    register u8*            head asm("v1");
    register GpQuadScratch* block asm("t1");
    register SVECTOR*       v asm("a2");
    register s32            i asm("t0");
    GpQuadCorner*           tbl;
    POLY_FT4*               prim;

    if (arg2 >= 0 && Gp_State1C->field_4 < 2) {
        scratch  = (void**)G_SCRATCH_HEAD;
        head     = (u8*)*scratch - 0x38;
        block    = (GpQuadScratch*)head;
        *scratch = head;
        gte_SetTransMatrix(&GsWSMATRIX);
        i   = 0;
        v   = block->vec;
        tbl = D_80111E38;
        do {
            v->vx = tbl->x * arg1;
            v->vy = 0;
            v->vz = tbl->y * arg1;
            gte_SetRotMatrix(&D_80070F34);
            gte_ldv0(v);
            gte_rtv0_real();
            gte_stsv(v);
            *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->vx;
            tbl++;
            *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->vy;
            i++;
            *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->vz;
            v++;
        } while (i < 4);

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec[0]);
        gte_rtps_real();
        gte_stsxy(&block->sxy0);
        gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim       = (POLY_FT4*)D_80071190;
            D_80071190 = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg2 == 0) {
                setcode(prim, 0x2D);
            } else {
                prim->r0 = arg2;
                prim->g0 = arg2;
                prim->b0 = arg2;
            }
            prim->tpage = 0x48;
            prim->clut  = 0x4283;
            prim->u0    = 0xC0;
            prim->v0    = 0x98;
            prim->v1    = 0x98;
            prim->u2    = 0xC0;
            prim->u1    = 0xF7;
            prim->v2    = 0xCF;
            prim->u3    = 0xF7;
            prim->v3    = 0xCF;
            setSemiTrans(prim, 1);
            prim->x0 = block->sxy0.vx;
            prim->y0 = block->sxy0.vy;
            prim->x1 = block->sxy1.vx;
            prim->y1 = block->sxy1.vy;
            prim->x2 = block->sxy2.vx;
            prim->y2 = block->sxy2.vy;
            prim->x3 = block->sxy3.vx;
            prim->y3 = block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
    }
}

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
