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

extern s32 Gp_LcgState;

void Gp_DrawEffSprite81(Task* arg0)
{
    register void**           scratch asm("a2");
    register u8*              head asm("a3");
    register GpEffFt4Scratch* block asm("t1");
    GameActorExt*             extra;
    register GsCOORDINATE2*   coord asm("a1");
    register GpEffWork*       mem asm("a0");
    POLY_FT4*                 prim;
    s16                       x;
    s16                       y;
    u16                       t;
    u16                       vz;
    s32                       len;
    s32                       code;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    extra                                     = arg0->extra;
    head                                      = *scratch;
    coord                                     = (GsCOORDINATE2*)extra->field_8;
    mem                                       = arg0->spawnArg2;
    ((GpEffFt4Scratch*)(head - 0x18))->vec.vx = *(u16*)&coord->workm.t[0];
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpEffFt4Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
    gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        COMPILER_BARRIER();
        len  = 9;
        code = 0x2D;
        TOUCH_REG2(len, code);
        setlen(prim, len);
        setcode(prim, code);
        if (mem->field_26 != 0) {
            setcode(prim, 0x2F);
        }
        prim->tpage = 0x29;
        prim->clut  = ((((GpEffClutOff*)&D_80112964[mem->field_2A])->field_4 >> 4) & 0x3F) | 0x4280;
        t           = mem->field_22;
        prim->v0    = 0xB8;
        prim->u0    = (t * 8) & 0x70;
        t           = mem->field_22;
        prim->v1    = 0xB8;
        prim->u1    = ((t * 8) & 0x70) + 0xF;
        t           = mem->field_22;
        prim->v2    = 0xC7;
        prim->u2    = (t * 8) & 0x70;
        t           = mem->field_22;
        prim->v3    = 0xC7;
        prim->u3    = ((t * 8) & 0x70) + 0xF;
        block->size = ((mem->field_24 * 0xF) / block->otz) >> 1;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

void func_800F7AD4(GsCOORDINATE2* arg0, s32 arg1, s16 arg2, u16 arg3)
{
    register GsCOORDINATE2* coord asm("t7");
    register void**         scratch asm("a0");
    register GpQuadScratch* block asm("t1");
    register SVECTOR*       v asm("t0");
    register s32            i asm("t3");
    GpQuadCorner*           tbl;
    register u8*            head asm("v0");
    register MATRIX*        wm asm("t8");
    POLY_FT4*               prim;

    coord    = arg0;
    scratch  = (void**)G_SCRATCH_HEAD;
    i        = 0;
    wm       = &coord->workm;
    tbl      = D_80111E38;
    head     = (u8*)*scratch - 0x38;
    block    = (GpQuadScratch*)head;
    v        = block->vec;
    *scratch = block;
    do {
        v->vx = tbl->x * arg1;
        v->vy = 0;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
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
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz    += 0x20;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x29;
        prim->clut  = getClut(arg3 * 0x110 + 0x10, 0x10A);
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        prim->r0    = arg2;
        prim->g0    = arg2;
        prim->b0    = arg2;
        prim->u0    = 0;
        prim->u1    = 0x37;
        prim->u2    = 0;
        prim->v2    = 0xFF;
        prim->u3    = 0x37;
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
}

void Gp_EffSprTask81(Task* arg0)
{
    GpEffWork*     mem;
    GameActorExt*  extra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    MATRIX*        world;
    s16            flag;
    s32            one;

    extra  = arg0->extra;
    mem    = arg0->spawnArg2;
    flag   = Gp_State1C->field_4;
    coord  = (GsCOORDINATE2*)extra->field_8;
    parent = mem->field_8;
    if (flag >= 2) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    Gp_UpdateCoord(parent);
    coord->workm = parent->workm;
    gte_SetRotMatrix(&parent->workm);
    gte_SetTransMatrix(&parent->workm);
    world = &Gfx_ViewWorldMtx;
    Gp_WorldToLocal(world, &coord->workm, &coord->coord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);

    switch (arg0->spawnArg1) {
        case 0:
            mem->field_24   = 0x280;
            mem->field_2A   = 1;
            mem->field_26   = 0;
            arg0->spawnArg1 = 1;
            if (Gp_State1C->field_4 != 0) {
                break;
            }
            Gp_SpawnEff(0x60042, coord, mem->field_24 + 0x22200 + mem->field_24, 0);
            break;
        case 1:
            Gp_DrawEffSprite81(arg0);
            if (Gp_State1C->field_4 != 0) {
                break;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                Gp_SpawnEff(0x60042, coord, mem->field_24 + 0x21000, 0);
            }
            mem->field_22++;
            break;
        case 2:
            if (Gp_State1C->field_4 == 0) {
                if (mem->field_20 == 0) {
                    Gp_SpawnEff(0x60042, coord, mem->field_24 + 0x22200, 0);
                    mem->field_20        = 1;
                    mem->field_22        = 0;
                    mem->field_24      >>= 2;
                    one                  = ONE;
                    *(s32*)&coord->coord = one;
                    m                    = &coord->coord;
                    *(s32*)&m->m[0][2]   = 0;
                    *(s32*)&m->m[1][1]   = one;
                    *(s32*)&m->m[2][0]   = 0;
                    m->m[2][2]           = one;
                }
                mem->field_22 += (u16)Display_State.field_8 & 1;
            }
            if (mem->field_22 < 0x10) {
                Gp_DrawEffQuadT29(coord, mem->field_24, mem->field_22 >> 1, mem->field_2A);
            } else {
                arg0->spawnArg1 = 4;
                break;
            }
            goto lcg;
        case 3:
            if (Gp_State1C->field_4 == 0 && mem->field_20 == 0) {
                Gp_SpawnEff(0x60042, coord, mem->field_24 + 0x22200, 0);
                mem->field_20   = 1;
                mem->field_22   = 0;
                mem->field_24 >>= 2;
            }
            mem->field_22++;
            if (mem->field_22 >= 0x10) {
                arg0->spawnArg1 = 4;
                break;
            }
        lcg:
            if (Gp_State1C->field_4 != 0) {
                break;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 3U) == 0) {
                Gp_SpawnEff(0x6003F, coord, mem->field_24, 0);
            }
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

static const s32 s_jtbl_pad = 0;
