#include "common.h"

#include "gameplay/3A34.h"
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
#include <psyq/memory.h>

#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32     Gp_LcgState;
extern SVECTOR D_801124DC[];
extern SVECTOR D_801125EC[];
extern SVECTOR D_801126FC[];
extern SVECTOR D_8011280C[];

void func_800EF0E0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800F1BEC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_800F68AC(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_800F6560(GsCOORDINATE2* arg0, s32 arg1, u32 arg2);

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

void func_800EE210(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s32               rng;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        if (mem->field_22 == 0) {
            rng           = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState   = rng;
            mem->field_26 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
        }
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
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
            prim->tpage = 0x2A;
            prim->clut  = 0x4340;
            prim->u0    = mem->field_22 * 40;
            prim->v0    = 0xD8;
            prim->u1    = mem->field_22 * 40 + 0x27;
            prim->v1    = 0xD8;
            prim->u2    = mem->field_22 * 40;
            prim->v2    = 0xFF;
            prim->u3    = mem->field_22 * 40 + 0x27;
            prim->v3    = 0xFF;
            block->dx   = (((mem->field_26 * 23) / block->otz) * rsin(mem->field_24)) >> 12;
            block->dy   = (((mem->field_26 * 23) / block->otz) * rcos(mem->field_24)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->field_26 * 23) / block->otz) * rsin(mem->field_24 + 0x400)) >> 12;
            block->dy   = (((mem->field_26 * 23) / block->otz) * rcos(mem->field_24 + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 < 2) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_800EE72C(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s32               rng;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            rng           = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = (((u32)rng >> 16) & 0x800) - 0x200;
            Gp_LcgState   = rng;
            mem->field_26 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
            arg0->state   = 1;
        }
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
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
            prim->clut  = 0x4288;
            prim->u0    = mem->field_22 * 32;
            prim->v0    = 0x38;
            prim->u1    = mem->field_22 * 32 + 0x1F;
            prim->v1    = 0x38;
            prim->u2    = mem->field_22 * 32;
            prim->v2    = 0x57;
            prim->u3    = mem->field_22 * 32 + 0x1F;
            prim->v3    = 0x57;
            block->dx   = (((mem->field_26 * 31) / block->otz) * rsin(mem->field_24)) >> 12;
            block->dy   = (((mem->field_26 * 31) / block->otz) * rcos(mem->field_24)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->field_26 * 31) / block->otz) * rsin(mem->field_24 + 0x400)) >> 12;
            block->dy   = (((mem->field_26 * 31) / block->otz) * rcos(mem->field_24 + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 < 2) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_800EEC14(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffLineScratch* block;
    GpEffLineScratch* vecp;
    LINE_G2*          prim;
    s16               flag;
    s16               c;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = ((u32)Gp_LcgState >> 16) % 3 + 1;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_10 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_14 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            arg0->state   = 1;
        }
        scratch                                     = (void**)G_SCRATCH_HEAD;
        head                                        = *scratch;
        ((GpEffLineScratch*)(head - 0x20))->vec0.vx = *(u16*)&coord->workm.t[0];
        block                                       = (GpEffLineScratch*)(head - 0x20);
        block->vec0.vy                              = *(u16*)&coord->workm.t[1];
        vz                                          = *(u16*)&coord->workm.t[2];
        *scratch                                    = block;
        block->vec0.vz                              = vz;
        vecp                                        = block;
        gte_SetRotMatrix(&mem->field_8->coord);
        gte_ldv0((SVECTOR*)&mem->field_10);
        gte_rtv0_real();
        gte_stsv(&((GpEffLineScratch*)(head - 0x20))->vec1);
        gte_SetRotMatrix(&D_80070F34);
        gte_ldv0(&((GpEffLineScratch*)(head - 0x20))->vec1);
        gte_rtv0_real();
        gte_stsv(&((GpEffLineScratch*)(head - 0x20))->vec1);
        gte_lddp((mem->field_22 << 11) + 0x1000);
        gte_ldsv(&((GpEffLineScratch*)(head - 0x20))->vec1);
        gte_gpf12_real();
        gte_stsv(&((GpEffLineScratch*)(head - 0x20))->vec1);
        *(u16*)&block->vec1.vx = *(u16*)&block->vec1.vx + *(u16*)&((GpEffLineScratch*)(head - 0x20))->vec0.vx;
        *(u16*)&block->vec1.vy = *(u16*)&block->vec1.vy + *(u16*)&block->vec0.vy;
        *(u16*)&block->vec1.vz = *(u16*)&block->vec1.vz + *(u16*)&block->vec0.vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec0);
        gte_rtps_real();
        gte_stsxy(&((GpEffLineScratch*)(head - 0x20))->sxy0);
        gte_stflg(&((GpEffLineScratch*)(head - 0x20))->flag);
        if (block->flag >= 0) {
            gte_ldv0(&((GpEffLineScratch*)(head - 0x20))->vec1);
            gte_rtps_real();
            gte_stsxy(&((GpEffLineScratch*)(head - 0x20))->sxy1);
            gte_stflg(&((GpEffLineScratch*)(head - 0x20))->flag);
            if (block->flag >= 0) {
                gte_stszotz(&((GpEffLineScratch*)(head - 0x20))->otz);
                block->otz = block->otz + 1;
                prim       = (LINE_G2*)D_80071190;
                D_80071190 = (DR_TPAGE*)(prim + 1);
                setlen(prim, 4);
                setcode(prim, 0x50);
                c        = 0xFF - ((u16)mem->field_22 << 6);
                prim->r0 = 0;
                prim->g0 = 0;
                prim->b0 = 0;
                prim->r1 = c;
                prim->g1 = c >> mem->field_24;
                prim->b1 = c >> 3;
                prim->x0 = *(u16*)&block->sxy0.vx;
                prim->y0 = *(u16*)&block->sxy0.vy;
                prim->x1 = *(u16*)&block->sxy1.vx;
                prim->y1 = *(u16*)&block->sxy1.vy;
                addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            }
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
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

void func_800EF4D0(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s16               val;
    s32               rng;
    s32               t2;
    s32               quot;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        if (arg0->state == 0) {
            rng           = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState   = rng;
            mem->field_26 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            if (arg0->spawnArg1 & 0xF0000) {
                val = (arg0->spawnArg1 >> 16) & 0xF;
            } else {
                val = 1;
            }
            mem->field_28 = val;
            if (arg0->spawnArg1 & 0x1000) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            }
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
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
            prim->tpage = 0x28;
            prim->clut  = 0x4242;
            quot        = mem->field_22 / mem->field_28;
            prim->v0    = 0;
            prim->u0    = quot * 24 + 0x30;
            quot        = mem->field_22 / mem->field_28;
            prim->v1    = 0;
            prim->u1    = quot * 24 + 0x47;
            quot        = mem->field_22 / mem->field_28;
            prim->v2    = 0x17;
            prim->u2    = quot * 24 + 0x30;
            quot        = mem->field_22 / mem->field_28;
            prim->v3    = 0x17;
            prim->u3    = quot * 24 + 0x47;
            block->dx   = (((mem->field_26 * 23) / block->otz) * rsin(mem->field_24)) >> 12;
            block->dy   = (((mem->field_26 * 23) / block->otz) * rcos(mem->field_24)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->field_26 * 23) / block->otz) * rsin(mem->field_24 + 0x400)) >> 12;
            block->dy   = (((mem->field_26 * 23) / block->otz) * rcos(mem->field_24 + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        coord->coord.t[0] += mem->field_10;
        coord->coord.t[1] += mem->field_12;
        t2                 = coord->coord.t[2] + mem->field_14;
        coord->flg         = 0;
        coord->coord.t[2]  = t2;
        mem->field_22++;
        if (mem->field_22 <= mem->field_28 * 4 - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_800EFBC4(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s32               t2;
    s32               quot;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            mem->field_26 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_28 = (((u32)Gp_LcgState >> 16) & 1) + 1;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = ((u32)Gp_LcgState >> 14) & 0x7C;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_14 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            gte_SetRotMatrix(&mem->field_8->coord);
            gte_ldv0((SVECTOR*)&mem->field_10);
            gte_rtv0_real();
            gte_stsv((SVECTOR*)&mem->field_10);
            arg0->state = 1;
        }
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
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
            prim->tpage = 0x28;
            prim->clut  = 0x4253;
            quot        = mem->field_22 / mem->field_28;
            prim->v0    = 0x18;
            prim->u0    = quot * 32;
            quot        = mem->field_22 / mem->field_28;
            prim->v1    = 0x18;
            prim->u1    = quot * 32 + 0x1F;
            quot        = mem->field_22 / mem->field_28;
            prim->v2    = 0x37;
            prim->u2    = quot * 32;
            quot        = mem->field_22 / mem->field_28;
            prim->v3    = 0x37;
            prim->u3    = quot * 32 + 0x1F;
            block->dx   = (((mem->field_26 * 31) / block->otz) * rsin(mem->field_24)) >> 12;
            block->dy   = (((mem->field_26 * 31) / block->otz) * rcos(mem->field_24)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->field_26 * 31) / block->otz) * rsin(mem->field_24 + 0x400)) >> 12;
            block->dy   = (((mem->field_26 * 31) / block->otz) * rcos(mem->field_24 + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        coord->coord.t[0] += mem->field_10;
        coord->coord.t[1] += mem->field_12;
        t2                 = coord->coord.t[2] + mem->field_14;
        coord->flg         = 0;
        coord->coord.t[2]  = t2;
        mem->field_22++;
        if (mem->field_22 <= mem->field_28 * 8 - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_800F02B4(Task* arg0)
{
    SVECTOR        delta;
    SVECTOR        dir;
    SVECTOR        pos;
    VECTOR         vec;
    VECTOR         tmp;
    GameActorExt*  extra;
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    SVECTOR*       vel;
    s16            flag;
    s32            t2;
    s32            scale;
    u16            tx;
    u16            ty;
    u16            tz;
    s32            dz;

    extra = (GameActorExt*)arg0->extra;
    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)extra->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto release;
    }
    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        extra->field_C &= 0xFF7F;
        switch (arg0->spawnArg1) {
            case 1:
            default:
                mem->field_24 = 0xD4;
                mem->field_26 = 0x14;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 2:
                mem->field_24 = 0x100;
                mem->field_26 = 0x14;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 3:
                mem->field_24 = 0x114;
                mem->field_26 = 0xF;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 30:
            case 31:
            case 32:
                mem->field_24 = 0x114;
                mem->field_26 = 0xA;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 5:
            case 33:
                mem->field_24 = 0xD4;
                mem->field_26 = 0x14;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 9:
                mem->field_26 = 0x14;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 16:
            case 20:
            case 21:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
                mem->field_24 = 0x114;
                mem->field_26 = 0xF;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 17:
                mem->field_24 = 0x114;
                mem->field_26 = 5;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0xFF80 - (((u32)Gp_LcgState >> 16) & 0x3F);
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 13:
            case 14:
            case 15:
                mem->field_24 = 0xBF;
                mem->field_26 = 0x14;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 23:
                mem->field_24 = 0xBF;
                mem->field_26 = 0x14;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0xFFA0 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = -(((u32)Gp_LcgState >> 16) & 0x7F);
                memset(&tmp, 0, 0x10);
                tmp.vx = mem->field_10;
                tmp.vy = mem->field_12;
                tmp.vz = mem->field_14;
                vec    = tmp;
                ApplyTransposeMatrixLV(&coord->coord, &vec, &vec);
                mem->field_10 = vec.vx;
                mem->field_12 = vec.vy;
                mem->field_14 = vec.vz;
                break;
            case 11:
                mem->field_24 = 0x60;
                mem->field_26 = 0x14;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = ((((u32)Gp_LcgState >> 16) & 0x1F) + 0x10);
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 12:
                mem->field_24 = 0x80;
                mem->field_26 = 0xF;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x3F);
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
            case 37:
                mem->field_24 = 0x60;
                mem->field_26 = 0x14;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0xFFF0 - (((u32)Gp_LcgState >> 16) & 0x1F);
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                break;
        }
        VectorNormalSS((SVECTOR*)&mem->field_10, (SVECTOR*)&mem->field_10);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_18 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_1A = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_1C = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
        coord->flg    = 0;
        arg0->state   = 1;
        Gfx_RotMatrixX(&coord->coord, 0x800, 0);
        return;
    }
    Gfx_RotMatrixXYZ(&coord->coord, (SVECTOR*)&mem->field_18, 0);
    MatrixNormal(&coord->coord, &coord->coord);
    gte_lddp(*(u16*)&mem->field_24);
    vel = (SVECTOR*)&mem->field_10;
    gte_ldsv(vel);
    gte_gpf12_real();
    gte_stsv(&delta);
    coord->coord.t[0] += delta.vx;
    coord->coord.t[1] += delta.vy;
    t2                 = coord->coord.t[2] + delta.vz;
    coord->flg         = 0;
    coord->coord.t[2]  = t2;
    gte_SetRotMatrix(&D_80070F34);
    gte_ldv0(&delta);
    gte_rtv0_real();
    gte_stsv(&dir);
    tx             = *(u16*)&coord->workm.t[0];
    pos.vx         = tx;
    ty             = *(u16*)&coord->workm.t[1];
    pos.vy         = ty;
    tz             = *(u16*)&coord->workm.t[2];
    *(u16*)&dir.vx = *(u16*)&dir.vx + tx;
    *(u16*)&dir.vy = *(u16*)&dir.vy + ty;
    pos.vz         = tz;
    *(u16*)&dir.vz = *(u16*)&dir.vz + tz;
    if (func_800DE7CC(&dir, &pos, &dir, &pos) == 1) {
        register SVECTOR* r0 asm("a0");
        r0 = vel;
        __asm__ volatile("" ::"r"(r0));
        coord->coord.t[0] -= delta.vx;
        coord->coord.t[1] -= delta.vy;
        coord->coord.t[2] -= delta.vz;
        __asm__ volatile("" ::: "memory");
        {
            u16          t10;
            u16          t11;
            register s32 t12 asm("a1");
            s32          sum;
            t10 = *(volatile u16*)&pos.vx;
            t11 = *(volatile u16*)&mem->field_10;
            t12 = *(volatile u16*)&mem->field_12;
            sum = ((s32)(t10 << 16) >> 17) + ((s32)(t11 << 16) >> 17);
            __asm__ volatile("" ::"r"(t12));
            mem->field_10 = sum;
            t12         <<= 16;
            t12         >>= 17;
            __asm__ volatile("" ::: "memory");
            mem->field_12 = *(u16*)&pos.vy + t12;
        }
        dz            = (s32)(*(u16*)&mem->field_14 << 16) >> 17;
        mem->field_14 = ((s32)(*(u16*)&pos.vz << 16) >> 17) + dz;
        VectorNormalSS(vel, vel);
        scale         = (mem->field_24 * 2) / 3;
        mem->field_24 = scale;
        gte_lddp(scale);
        gte_ldsv(vel);
        gte_gpf12_real();
        gte_stsv(&delta);
        coord->coord.t[0] += delta.vx;
        coord->coord.t[1] += delta.vy;
        coord->coord.t[2] += delta.vz;
    } else {
        mem->field_12 += 0x180;
    }
    mem->field_22++;
    if (mem->field_26 < mem->field_22) {
        if (Display_State.field_8 & 1) {
            extra->field_C &= 0xFF7F;
        } else {
            extra->field_C |= 0x80;
        }
        if (mem->field_26 * 2 < mem->field_22) {
            goto release;
        }
    }
    return;
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

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

void func_800F1638(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    GpEffTileScratch* block;
    TILE*             prim;
    s16               c;

    coord                  = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    *(u8**)G_SCRATCH_HEAD -= 0x14;
    block                  = (GpEffTileScratch*)*(u8**)G_SCRATCH_HEAD;
    mem                    = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        if (arg0->spawnArg1 != 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_10 = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_14 = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
        } else {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_10 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x100;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_14 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);

            gte_SetRotMatrix(&mem->field_8->coord);
            gte_ldv0((SVECTOR*)&mem->field_10);
            gte_rtv0_real();
            gte_stsv((SVECTOR*)&mem->field_10);

            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = ((u32)Gp_LcgState >> 16) % 3 + 1;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_26 = (((u32)Gp_LcgState >> 16) & 1) + 1;
        }
        arg0->state = 1;
    }
    coord->coord.t[0] += mem->field_10;
    coord->coord.t[1] += mem->field_12;
    coord->coord.t[2] += mem->field_14;
    coord->flg         = 0;
    block->vec.vx      = *(u16*)&coord->workm.t[0];
    block->vec.vy      = *(u16*)&coord->workm.t[1];
    block->vec.vz      = *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&block->sxy);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz = block->otz + 1;
        prim       = (TILE*)D_80071190;
        D_80071190 = (DR_TPAGE*)(prim + 1);
        setlen(prim, 3);
        setcode(prim, 0x60);
        c        = 0xFF - (u16)mem->field_22 * 0x10;
        prim->w  = mem->field_26;
        prim->h  = mem->field_26;
        prim->r0 = c;
        prim->g0 = c >> mem->field_24;
        prim->b0 = c >> 3;
        prim->x0 = block->sxy.vx;
        prim->y0 = block->sxy.vy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
    }
    *(u8**)G_SCRATCH_HEAD += 0x14;
    mem->field_22++;
    if (mem->field_22 >= 8) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

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

void func_800F1FF4(Task* arg0)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    GsCOORDINATE2*    coord;
    GpEffWork*        mem;
    POLY_FT4*         prim;
    GpEffUv8*         rec;
    s16               flag;
    s16               scale;
    s16               step;
    s32               rng;
    s32               i;
    s32               n;
    s32               t2;
    s32               tmp;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            scale = 0x200;
            if (arg0->spawnArg1 & 0xFFF) {
                scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            }
            mem->field_24 = scale;
            rng           = Gp_LcgState * 5 + 0x71357911;
            mem->field_26 = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState   = rng;
            if (arg0->spawnArg1 & 0xF000) {
                step = (arg0->spawnArg1 >> 12) & 0xF;
            } else {
                step = 2;
            }
            mem->field_28 = step;
            mem->field_2A = (s32)(*(u16*)&mem->field_24 << 16) >> 23;
            tmp           = ((GpEffSpawnArgHi*)&arg0->spawnArg1)->field_3;
            mem->field_20 = tmp & 0xF;
            if (mem->field_20 != 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                gte_lddp(mem->field_24 << 3);
                gte_ldsv((SVECTOR*)&mem->field_10);
                gte_gpf12_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                gte_lddp(mem->field_20 << 12);
                gte_ldsv((SVECTOR*)&mem->field_10);
                gte_gpf12_real();
                gte_stsv((SVECTOR*)&mem->field_10);
                gte_SetRotMatrix(&mem->field_8->coord);
                gte_ldv0((SVECTOR*)&mem->field_10);
                gte_rtv0_real();
                gte_stsv((SVECTOR*)&mem->field_10);
            } else if (!(arg0->spawnArg1 & 0xF0000000)) {
                n = Display_State.field_8 & 3;
                i = 0;
                if (n != 0) {
                    do {
                        func_800EA478(0x6005C, coord, ((s32)(*(u16*)&mem->field_24 << 16) >> 17) | 0x02001000, 0);
                        i += 1;
                    } while (i < n);
                }
                n = (u32)Display_State.field_8 % 3;
                i = 0;
                if (n > 0) {
                    do {
                        func_800EA478(0x6005C, coord, ((s32)(*(u16*)&mem->field_24 << 16) >> 17) | 0x01002000, 0);
                        i += 1;
                    } while (i < n);
                }
            }
            arg0->state = 1;
        }
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
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
            setcode(prim, 0x2C);
            rec         = &D_80111E48[mem->field_22 / mem->field_28];
            prim->code |= 3;
            prim->tpage = 0x29;
            prim->clut  = (rec->clutY << 6) | ((rec->clutX >> 4) & 0x3F);
            prim->u0    = rec->u;
            prim->v0    = rec->v;
            prim->u1    = rec->u + 0x27;
            prim->v1    = rec->v;
            prim->u2    = rec->u;
            prim->v2    = rec->v + 0x27;
            prim->u3    = rec->u + 0x27;
            prim->v3    = rec->v + 0x27;
            block->dx   = ((((s16)mem->field_24 * 0x27) / block->otz) * rsin(mem->field_26)) >> 12;
            block->dy   = ((((s16)mem->field_24 * 0x27) / block->otz) * rcos(mem->field_26)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = ((((s16)mem->field_24 * 0x27) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
            block->dy   = ((((s16)mem->field_24 * 0x27) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        coord->coord.t[0] += mem->field_10;
        coord->coord.t[1] += mem->field_12;
        t2                 = coord->coord.t[2] + mem->field_14;
        coord->flg         = 0;
        coord->coord.t[2]  = t2;
        mem->field_24     += mem->field_2A;
        mem->field_22++;
        if (mem->field_22 <= mem->field_28 * 12 - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F289C);

void func_800F3414(Task* arg0)
{
    void**                     scratch;
    u8*                        head;
    register GpEffBeamScratch* vecp asm("v0");
    GpEffBeamScratch*          block;
    GsCOORDINATE2*             coord;
    GpEffWork*                 mem;
    POLY_FT4*                  prim;
    u16                        size;
    s16                        scale;
    s32                        rng;

    coord    = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vecp     = (GpEffBeamScratch*)(head - 0x1C);
    block    = vecp;
    *scratch = vecp;
    mem      = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy                              = *(u16*)&coord->workm.t[1];
    block->vec.vz                              = *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        block->otz = block->otz + 1;
        prim       = (POLY_FT4*)D_80071190;
        D_80071190 = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        size = D_80112934[mem->field_22].w;
        if (arg0->state == 0) {
            if (arg0->spawnArg1 & 0xFFF) {
                scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            } else {
                scale = 0x200;
            }
            mem->field_24 = scale;
            rng           = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState   = rng;
            mem->field_26 = ((u32)rng >> 16) & 0xFFF;
            arg0->state   = 1;
        }
        prim->code |= 3;
        prim->tpage = ((D_80112934[mem->field_22].tpageX & 0x3FF) >> 6) | 0x20;
        prim->clut  = (D_80112934[mem->field_22].clutY << 6) | ((D_80112934[mem->field_22].clutX >> 4) & 0x3F);
        prim->u0    = D_80112934[mem->field_22].u;
        prim->v0    = D_80112934[mem->field_22].v;
        prim->u1    = D_80112934[mem->field_22].u + size;
        prim->v1    = D_80112934[mem->field_22].v;
        prim->u2    = D_80112934[mem->field_22].u;
        prim->v2    = D_80112934[mem->field_22].v + size;
        prim->u3    = D_80112934[mem->field_22].u + size;
        prim->v3    = D_80112934[mem->field_22].v + size;
        block->dx   = ((((s16)size * mem->field_24) / block->otz) * rsin(mem->field_26)) >> 12;
        block->dy   = ((((s16)size * mem->field_24) / block->otz) * rcos(mem->field_26)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = ((((s16)size * mem->field_24) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
        block->dy   = ((((s16)size * mem->field_24) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
    mem->field_22++;
    if (mem->field_22 >= 4) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void func_800F3A78(Task* arg0)
{
    GsCOORDINATE2              hit;
    GpEffWork*                 mem;
    GsCOORDINATE2*             coord;
    void**                     scratch;
    u8*                        head;
    register GpEffBeamScratch* vecp asm("v0");
    GpEffBeamScratch*          block;
    POLY_FT4*                  prim;
    s16                        flag;
    s32                        rng;
    s16                        scale;
    s16                        step;
    s32                        col;
    s32                        tmp;
    u32                        param;
    register u16               vx asm("v0");
    u16                        vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    param = 0x80;
    if (flag >= 2) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (mem->field_20 == 0) {
        scale = 0x200;
        if (arg0->spawnArg1 & 0xFFF) {
            scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        }
        mem->field_24 = scale;
        rng           = Gp_LcgState * 5 + 0x71357911;
        mem->field_26 = ((u32)rng >> 16) & 0xFFF;
        Gp_LcgState   = rng;
        if (arg0->spawnArg1 & 0xF000) {
            step = (arg0->spawnArg1 >> 12) & 0xF;
        } else {
            step = 1;
        }
        mem->field_28 = step;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_2A = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_10 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_12 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_14 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
        mem->field_20++;
    }
    Gp_UpdateCoord(coord);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    __asm__ volatile("" ::"r"(head));
    vx                                         = *(u16*)&coord->workm.t[0];
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
    vecp                                       = (GpEffBeamScratch*)(head - 0x1C);
    block                                      = vecp;
    block->vec.vy                              = *(u16*)&coord->workm.t[1];
    vz                                         = *(u16*)&coord->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        block->otz = block->otz + 1;
        prim       = (POLY_FT4*)D_80071190;
        D_80071190 = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (mem->field_22 >= 0x18) {
            col = (0x1F - mem->field_22) * 16;
            __asm__ volatile("" : "=r"(tmp) : "0"(col));
            param    = (u8)tmp;
            prim->r0 = col;
            prim->g0 = col;
            prim->b0 = col;
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = 0x28;
        prim->code |= 2;
        prim->clut  = 0x428A;
        prim->u0    = ((mem->field_22 / mem->field_28) % 6) * 16;
        prim->v0    = 0x58;
        prim->u1    = ((mem->field_22 / mem->field_28) % 6) * 16 + 0xF;
        prim->v1    = 0x58;
        prim->u2    = ((mem->field_22 / mem->field_28) % 6) * 16;
        prim->v2    = 0x67;
        prim->u3    = ((mem->field_22 / mem->field_28) % 6) * 16 + 0xF;
        prim->v3    = 0x67;
        block->dx   = (((mem->field_24 * 15) / block->otz) * rsin(mem->field_26)) >> 12;
        block->dy   = (((mem->field_24 * 15) / block->otz) * rcos(mem->field_26)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = (((mem->field_24 * 15) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
        block->dy   = (((mem->field_24 * 15) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
    if (Gp_State1C->field_4 != 0) {
        return;
    }
    coord->coord.t[0] += mem->field_10;
    coord->coord.t[1] += mem->field_12;
    coord->coord.t[2] += mem->field_14;
    coord->flg         = 0;
    Gp_UpdateCoord(coord);
    mem->field_12 += 5;
    mem->field_26 += mem->field_2A;
    mem->field_22++;
    if (mem->field_22 >= 0x1F) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (func_800EA02C(coord, &hit) == 1) {
        func_800F6560(&hit, (s32)(*(u16*)&mem->field_24 << 16) >> 17, param);
    }
    if (coord->coord.t[1] > hit.coord.t[1]) {
        coord->coord.t[1] -= mem->field_12 * 2;
        mem->field_12      = -((s32)(*(u16*)&mem->field_12 << 16) >> 17);
        mem->field_10      = (s32)(*(u16*)&mem->field_10 << 16) >> 17;
        mem->field_14      = (s32)(*(u16*)&mem->field_14 << 16) >> 17;
    }
}

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

void func_800F52B4(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u16 arg3)
{
    void**                     scratch;
    register u8*               head asm("a2");
    GpEffBeamScratch*          block;
    register u16               vx asm("v0");
    register GpEffBeamScratch* vecp asm("v0");
    register s32*              otzp asm("v0");
    POLY_G4*                   quad;
    POLY_G3*                   tri;
    s32                        ang;
    s32                        ang2;
    s32                        rng;
    s32                        base;
    u8                         r;
    u8                         g;
    u8                         b;
    u16                        vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    vx                                         = *(u16*)&arg0->workm.t[0];
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
    vecp                                       = (GpEffBeamScratch*)(head - 0x1C);
    block                                      = vecp;
    block->vec.vy                              = *(u16*)&arg0->workm.t[1];
    vz                                         = *(u16*)&arg0->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        otzp = &((GpEffBeamScratch*)(head - 0x1C))->otz;
        gte_stszotz(otzp);
        block->otz++;
        block->dx = (arg1 << 8) / block->otz;
        block->dy = (arg1 << 7) / block->otz;
        r         = arg2 * ((arg3 >> 8) & 0xF);
        g         = arg2 * ((arg3 >> 4) & 0xF);
        b         = arg2 * (arg3 & 0xF);
        ang       = 0;
        do {
            quad       = (POLY_G4*)D_80071190;
            D_80071190 = (DR_TPAGE*)(quad + 1);
            setlen(quad, 8);
            setcode(quad, 0x38);
            quad->r0 = r;
            quad->r1 = r;
            quad->g0 = g;
            quad->b0 = b;
            quad->g1 = g;
            quad->b1 = b;
            quad->r2 = 0;
            quad->g2 = 0;
            quad->b2 = 0;
            quad->r3 = 0;
            quad->g3 = 0;
            quad->b3 = 0;
            quad->x0 = *(u16*)&block->sxy.vx + ((block->dx * rsin(ang)) >> 12);
            ang2     = ang + 0x100;
            quad->y0 = *(u16*)&block->sxy.vy + ((block->dx * rcos(ang)) >> 12);
            quad->x1 = *(u16*)&block->sxy.vx + ((block->dx * rsin(ang2)) >> 12);
            quad->y1 = *(u16*)&block->sxy.vy + ((block->dx * rcos(ang2)) >> 12);
            quad->x2 = *(u16*)&block->sxy.vx + ((block->dy * rsin(ang)) >> 12);
            quad->y2 = *(u16*)&block->sxy.vy + ((block->dy * rcos(ang)) >> 12);
            quad->x3 = *(u16*)&block->sxy.vx + ((block->dy * rsin(ang2)) >> 12);
            quad->y3 = *(u16*)&block->sxy.vy + ((block->dy * rcos(ang2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    quad);
            ang = ang2;
            Gp_AddTpageShift((P_TAG*)quad, 1, block->otz);
        } while (ang < 0x1000);
        if (arg3 & 0x1000) {
            block->dx = 0x12000 / block->otz;
            block->dy = 0x2400 / block->otz;
            ang       = 0;
            do {
                rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                tri               = (POLY_G3*)D_80071190;
                D_80071190        = (DR_TPAGE*)(tri + 1);
                setlen(tri, 6);
                setcode(tri, 0x30);
                tri->r0 = r;
                tri->g0 = g;
                tri->b0 = b;
                tri->r1 = 0;
                tri->g1 = 0;
                tri->b1 = 0;
                tri->r2 = 0;
                tri->g2 = 0;
                tri->b2 = 0;
                base    = ang + (((u32)rng >> 16) & 0x300);
                tri->x0 = *(u16*)&block->sxy.vx;
                tri->y0 = *(u16*)&block->sxy.vy;
                tri->x1 = *(u16*)&block->sxy.vx + ((block->dx * rsin(base)) >> 12) +
                          ((block->dy * rsin(base + 0xC00)) >> 12);
                tri->y1 = *(u16*)&block->sxy.vy + ((block->dx * rcos(base)) >> 12) +
                          ((block->dy * rcos(base + 0xC00)) >> 12);
                tri->x2 = *(u16*)&block->sxy.vx + ((block->dx * rsin(base)) >> 12) +
                          ((block->dy * rsin(base + 0x400)) >> 12);
                tri->y2 = *(u16*)&block->sxy.vy + ((block->dx * rcos(base)) >> 12) +
                          ((block->dy * rcos(base + 0x400)) >> 12);
                addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        tri);
                ang += 0x400;
                Gp_AddTpageShift((P_TAG*)tri, 1, block->otz);
            } while (ang < 0x1000);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

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

void func_800F5E1C(Task* arg0)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    s16               count;
    s16               step;
    u16               vz;
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    POLY_FT4*         prim;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 >= 2) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_24 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
        if (arg0->spawnArg1 & 0xF000) {
            count = (arg0->spawnArg1 >> 12) & 0xF;
        } else {
            count = 1;
        }
        mem->field_28 = count;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_12 = -(((u32)Gp_LcgState >> 16) & 0xF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_14 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
        if (arg0->spawnArg1 < 0) {
            if (mem->field_26 & 1) {
                func_800EA478(0x60054, coord, ((mem->field_24 * 3) >> 2) + 0x3000, NULL);
            }
            if (!(mem->field_26 & 3)) {
                func_800EA478(0x60054, coord, ((mem->field_24 * 3) >> 2) + 0x3000, NULL);
            }
        }
        arg0->state = 1;
    }

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                      = (GpEffBeamScratch*)(head - 0x1C);
    block->vec.vy                              = *(u16*)&coord->workm.t[1];
    vz                                         = *(u16*)&coord->workm.t[2];
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
        setcode(prim, 0x2C);
        prim->r0    = 0x68;
        prim->g0    = 0x70;
        prim->b0    = 0x38;
        prim->tpage = 0x28;
        setSemiTrans(prim, 1);
        prim->clut = 0x4253;
        prim->u0   = (mem->field_22 / mem->field_28) << 5;
        prim->v0   = 0x18;
        prim->u1   = ((mem->field_22 / mem->field_28) << 5) + 0x1F;
        prim->v1   = 0x18;
        prim->u2   = (mem->field_22 / mem->field_28) << 5;
        prim->v2   = 0x37;
        prim->u3   = ((mem->field_22 / mem->field_28) << 5) + 0x1F;
        prim->v3   = 0x37;

        block->dx = ((((s32)mem->field_24 * 31) / block->otz) * rsin(mem->field_26)) >> 12;
        block->dy = ((((s32)mem->field_24 * 31) / block->otz) * rcos(mem->field_26)) >> 12;
        prim->x0  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx = ((((s32)mem->field_24 * 31) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
        block->dy = ((((s32)mem->field_24 * 31) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
        prim->x1  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
    if (Gp_State1C->field_4 == 0) {
        coord->coord.t[0] += mem->field_10;
        coord->coord.t[1] += mem->field_12;
        coord->coord.t[2] += mem->field_14;
        coord->flg         = 0;
        step               = mem->field_22 + 1;
        mem->field_22      = step;
        if (step > (mem->field_28 * 8) - 1) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

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

void func_800F6D18(Task* arg0)
{
    SVECTOR        delta;
    SVECTOR        dir;
    SVECTOR        pos;
    VECTOR         scale2;
    VECTOR         scale;
    GameActorExt*  extra;
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    SVECTOR*       rot;
    MATRIX*        mtx;
    s32            state;
    s16            flag;
    s16            trans;
    s32            temp;
    u16            tx;
    u16            ty;
    u16            tz;
    s32            dz;

    extra  = (GameActorExt*)arg0->extra;
    mem    = arg0->spawnArg2;
    coord  = (GsCOORDINATE2*)extra->field_8;
    player = (GsCOORDINATE2*)((GameActorExt*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    flag   = Gp_State1C->field_4;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto release;
    }
    Gp_UpdateCoord(coord);
    mem->field_22++;
    state = arg0->state;
    switch (state) {
        case 0:
            extra->field_C &= 0xFF7F;
            mem->field_24   = 0x100;
            if (arg0->spawnArg1 & 0xFFF) {
                temp = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            } else {
                temp = 0x200;
            }
            mem->field_26 = temp;
            mem->field_28 = 0x800;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_10 = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = 0x400 - (((u32)Gp_LcgState >> 16) % 0xC00);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_14 = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
            VectorNormalSS((SVECTOR*)&mem->field_10, (SVECTOR*)&mem->field_10);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_18 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_1A = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_1C = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            coord->flg    = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            return;
        case 1:
            mtx = &coord->coord;
            rot = (SVECTOR*)&mem->field_18;
            Gfx_RotMatrixXYZ(mtx, rot, 0);
            MatrixNormal(mtx, mtx);
            gte_lddp(*(u16*)&mem->field_24);
            gte_ldsv((SVECTOR*)&mem->field_10);
            gte_gpf12_real();
            gte_stsv(&delta);
            coord->coord.t[0] += delta.vx;
            coord->coord.t[1] += delta.vy;
            coord->coord.t[2] += delta.vz;
            coord->flg         = 0;
            gte_SetRotMatrix(&D_80070F34);
            gte_ldv0(&delta);
            gte_rtv0_real();
            gte_stsv(&dir);
            tx             = *(u16*)&coord->workm.t[0];
            pos.vx         = tx;
            ty             = *(u16*)&coord->workm.t[1];
            pos.vy         = ty;
            tz             = *(u16*)&coord->workm.t[2];
            *(u16*)&dir.vx = *(u16*)&dir.vx + tx;
            *(u16*)&dir.vy = *(u16*)&dir.vy + ty;
            pos.vz         = tz;
            *(u16*)&dir.vz = *(u16*)&dir.vz + tz;
            if (func_800DE7CC(&dir, &pos, &dir, &pos) == state) {
                SVECTOR*          vel;
                register SVECTOR* r0 asm("a0");
                vel = (SVECTOR*)&mem->field_10;
                r0  = vel;
                __asm__ volatile("" ::"r"(r0));
                coord->coord.t[0] -= delta.vx;
                coord->coord.t[1] -= delta.vy;
                coord->coord.t[2] -= delta.vz;
                __asm__ volatile("" ::: "memory");
                {
                    u16          t10;
                    u16          t11;
                    register s32 t12 asm("a1");
                    s32          sum;
                    t10 = *(volatile u16*)&pos.vx;
                    t11 = *(volatile u16*)&mem->field_10;
                    t12 = *(volatile u16*)&mem->field_12;
                    sum = ((s32)(t10 << 16) >> 17) + ((s32)(t11 << 16) >> 17);
                    __asm__ volatile("" ::"r"(t12));
                    mem->field_10 = sum;
                    t12         <<= 16;
                    t12         >>= 17;
                    __asm__ volatile("" ::: "memory");
                    mem->field_12 = *(u16*)&pos.vy + t12;
                }
                dz            = (s32)(*(u16*)&mem->field_14 << 16) >> 17;
                mem->field_14 = ((s32)(*(u16*)&pos.vz << 16) >> 17) + dz;
                VectorNormalSS(vel, vel);
                mem->field_24 = (s32)(*(u16*)&mem->field_24 << 16) >> 17;
                gte_lddp(mem->field_24);
                gte_ldsv(vel);
                gte_gpf12_real();
                gte_stsv(&delta);
                coord->coord.t[0] += delta.vx;
                coord->coord.t[1] += delta.vy;
                coord->coord.t[2] += delta.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (!(mem->field_22 & 3)) {
                    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
                }
                func_800EA478(0x60055, coord, mem->field_26 + 0x12200, 0);
                gte_lddp(0x800);
                gte_ldsv(rot);
                gte_gpf12_real();
                gte_stsv(rot);
                if (((s16)mem->field_22 - mem->field_2A) < 8 && mem->field_24 < 0x20) {
                    extra->field_C |= 2;
                    mem->field_22   = 0;
                    arg0->state     = 2;
                    return;
                }
                mem->field_2A = mem->field_22;
                return;
            }
            if (mem->field_24 == 0) {
                return;
            }
            if (mem->field_22 >= 0x4C) {
                goto release;
            }
            if (player->coord.t[1] + 0x100 < coord->coord.t[1]) {
                mem->field_22 += 0xA;
            }
            Gp_UpdateCoord(coord);
            if (!(mem->field_22 & 3)) {
                func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            }
            mem->field_12 += 0x10000 / mem->field_24;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            if (!(((u32)Gp_LcgState >> 16) & 3)) {
                func_800EA478(0x60042, coord, mem->field_26 + 0x11000, 0);
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (!(((u32)Gp_LcgState >> 16) & 7)) {
                func_800EA478(0x60055, coord, mem->field_26 + 0x11000, 0);
            }
            if (mem->field_22 >= 0x33) {
                extra->field_C |= 2;
                if (mem->field_28 >= 0x41) {
                    trans         = mem->field_28 - 0x40;
                    mem->field_28 = trans;
                    Gp_SetObjTrans((GpObj20*)extra, trans, trans, trans);
                    return;
                }
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            if (!(mem->field_22 & 3)) {
                func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            }
            if (mem->field_22 >= 0x10) {
                goto release;
            }
            memset(&scale, 0, 0x10);
            scale.vx = 0x1000;
            scale.vy = (0x10 - mem->field_22) << 8;
            scale.vz = 0x1000;
            scale2   = scale;
            ScaleMatrix(&coord->coord, &scale2);
            coord->flg = 0;
            if (mem->field_28 >= 0x81) {
                trans         = mem->field_28 - 0x80;
                mem->field_28 = trans;
                Gp_SetObjTrans((GpObj20*)extra, trans, trans, trans);
            }
            if (mem->field_22 == 8) {
                func_800EA478(0x600A5, coord, mem->field_26 >= 0x100, 0);
            }
            return;
    }
    return;
release:
    Gp_ReleaseState1CMem(mem, arg0);
}
