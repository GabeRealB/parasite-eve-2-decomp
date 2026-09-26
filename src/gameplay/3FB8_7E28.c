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
#include "gte.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

void Gp_DrawEffSprite81(Task* arg0)
{
    GpRingScratch* block;
    TmdObject*     extra;
    GpCoord*       coord;
    GpEffWork*     mem;
    POLY_FT4*      prim;

    extra         = arg0->extra.tmd;
    coord         = extra->coords;
    mem           = arg0->spawnArg2;
    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = coord->workm.t[0];
    block->vec.vy = coord->workm.t[1];
    block->vec.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setShadeTex(prim, 1);
        setSemiTrans(prim, mem->angle);
        prim->tpage = 0x29;
        prim->clut  = ((D_80112964[1][mem->step] >> 4) & 0x3F) | 0x4280;
        prim->u0    = ((mem->age >> 1) & 7) * 16;
        prim->v0    = 0xB8;
        prim->u1    = (((mem->age >> 1) & 7) * 16) + 0xF;
        prim->v1    = 0xB8;
        prim->u2    = ((mem->age >> 1) & 7) * 16;
        prim->v2    = 0xC7;
        prim->u3    = (((mem->age >> 1) & 7) * 16) + 0xF;
        prim->v3    = 0xC7;
        block->step = ((mem->scale * 0xF) / block->otz) >> 1;
        prim->x0 = prim->x2 = block->sx - (u16)block->step;
        prim->x1 = prim->x3 = block->sx + (u16)block->step;
        prim->y0 = prim->y1 = block->sy - (u16)block->step;
        prim->y2 = prim->y3 = block->sy + (u16)block->step;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

void Gp_DrawEffSprite46(GpCoord* arg0, s32 arg1, s16 arg2, u16 arg3)
{
    GpCoord*          coord;
    register void**   scratch asm("a0");
    GpQuadScratch*    block;
    register SVECTOR* v asm("t0");
    register s32      i asm("t3");
    GpQuadCorner*     tbl;
    register u8*      head asm("v0");
    MATRIX*           wm;
    POLY_FT4*         prim;

    coord                                   = arg0;
    scratch                                 = SCRATCH_HEAD_ADDR;
    i                                       = 0;
    wm                                      = &coord->workm;
    tbl                                     = D_80111E38;
    head                                    = SCRATCH_HEAD_AT(scratch, u8) - 0x38;
    block                                   = (GpQuadScratch*)head;
    v                                       = block->vec;
    SCRATCH_HEAD_AT(scratch, GpQuadScratch) = block;
    do {
        v->vx = tbl->x * arg1;
        v->vy = 0;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        (u16) v->vx = (u16)v->vx + (u16)coord->workm.t[0];
        tbl++;
        (u16) v->vy = (u16)v->vy + (u16)coord->workm.t[1];
        i++;
        (u16) v->vz = (u16)v->vz + (u16)coord->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz    += 0x20;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

void Gp_EffSprTask81(Task* arg0)
{
    GpEffWork* mem;
    TmdObject* extra;
    GpCoord*   coord;
    GpCoord*   parent;
    MATRIX*    m;
    MATRIX*    world;
    s16        flag;
    s32        one;

    extra  = arg0->extra.tmd;
    mem    = arg0->spawnArg2;
    flag   = Gp_State1C->eventState;
    coord  = extra->coords;
    parent = mem->parent;
    if (flag >= 2) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    Gp_UpdateCoord(parent);
    coord->workm = parent->workm;
    gte_SetRotMatrix(&parent->workm);
    gte_SetTransMatrix(&parent->workm);
    world = &gGfxViewCoord.workm;
    Gp_WorldToLocal(world, &coord->workm, &coord->coord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);

    switch (arg0->spawnArg1) {
        case 0:
            mem->scale      = 0x280;
            mem->step       = 1;
            mem->angle      = 0;
            arg0->spawnArg1 = 1;
            if (Gp_State1C->eventState != 0) {
                break;
            }
            Gp_SpawnEff(0x60042, coord, mem->scale + 0x22200 + mem->scale, 0);
            break;
        case 1:
            Gp_DrawEffSprite81(arg0);
            if (Gp_State1C->eventState != 0) {
                break;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                Gp_SpawnEff(0x60042, coord, mem->scale + 0x21000, 0);
            }
            mem->age++;
            break;
        case 2:
            if (Gp_State1C->eventState == 0) {
                if (mem->index == 0) {
                    Gp_SpawnEff(0x60042, coord, mem->scale + 0x22200, 0);
                    mem->index           = 1;
                    mem->age             = 0;
                    mem->scale         >>= 2;
                    one                  = ONE;
                    *(s32*)&coord->coord = one;
                    m                    = &coord->coord;
                    MATRIX_PAIR(m, 0, 2) = 0;
                    MATRIX_PAIR(m, 1, 1) = one;
                    MATRIX_PAIR(m, 2, 0) = 0;
                    m->m[2][2]           = one;
                }
                mem->age += (u16)gDisplayState.animFrame & 1;
            }
            if (mem->age < 0x10) {
                Gp_DrawEffQuadT29(coord, mem->scale, mem->age >> 1, mem->step);
            } else {
                arg0->spawnArg1 = 4;
                break;
            }
            goto lcg;
        case 3:
            if (Gp_State1C->eventState == 0 && mem->index == 0) {
                Gp_SpawnEff(0x60042, coord, mem->scale + 0x22200, 0);
                mem->index   = 1;
                mem->age     = 0;
                mem->scale >>= 2;
            }
            mem->age++;
            if (mem->age >= 0x10) {
                arg0->spawnArg1 = 4;
                break;
            }
        lcg:
            if (Gp_State1C->eventState != 0) {
                break;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 3U) == 0) {
                Gp_SpawnEff(0x6003F, coord, mem->scale, 0);
            }
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

static const s32 s_jtbl_pad = 0;
