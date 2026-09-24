#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_toilet.h"
#include "rooms/room_common.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")

extern u8  D_801153F4;
extern s32 Gp_LcgState;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;
extern s16 D_dryfield_toilet_80181120[][3];

void func_dryfield_toilet_8017DCF0(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        if (D_801153F4 == 1) {
            return;
        }
        if (arg0->state == 0) {
            coord->sub        = mem->parent;
            coord->coord.t[0] = mem->pos.vx;
            coord->coord.t[1] = mem->pos.vy;
            coord->coord.t[2] = mem->pos.vz;
            coord->flg        = 0;
            arg0->state       = 1;
            mem->scale        = 0x30;
            mem->angle        = arg0->spawnArg1;
            if (((u16)mem->pos.vx | (u16)mem->pos.vy | (u16)mem->pos.vz) == 0) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vx = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vy = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vz = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
            }
            VectorNormalSS(&mem->pos, &mem->move);
        }
        Gp_UpdateCoord(coord);
        spawned = Gp_SpawnEff(0x6009F, coord, 0x11180, 0);
        if (spawned != NULL) {
            gte_lddp(mem->scale - mem->age);
            gte_ldsv(&mem->move);
            gte_gpf12_real();
            gte_stsv(&spawned->move);
        }
        mem->age++;
        if (mem->age < mem->angle) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_dryfield_toilet_8017DEF4(Task* arg0)
{
    GpEffWork*                   mem;
    GsCOORDINATE2*               coord;
    void**                       scratch;
    u8*                          head;
    DryfieldToiletSpriteScratch* block;
    POLY_FT4*                    prim;
    s32                          rng;
    s32                          temp;
    SVECTOR*                     vec;
    s32                          t2;
    u8*                          tmp;
    u16                          vx;
    u16                          vz;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    vx      = *(u16*)&coord->workm.t[0];
    tmp     = head - 0x18;
    SOFT_USE_REG(tmp); /* keeps the carve apart from `block`, so the head store takes the copy */
    block         = (DryfieldToiletSpriteScratch*)tmp;
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((DryfieldToiletSpriteScratch*)(head - 0x18))->vec);
    gte_rtps_real();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((DryfieldToiletSpriteScratch*)(head - 0x18))->sxy);
    gte_stszotz(&block->otz);
    if (((DryfieldToiletSpriteScratch*)(head - 0x18))->otz >= 0x11) {
        if (arg0->state == 0) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            mem->scale  = (u16)arg0->spawnArg1 & 0xFFF;
            mem->angle  = ((u32)rng >> 16) & 0xFFF;
            temp        = ((u16)arg0->spawnArg1 & 0xF000) << 16;
            Gp_LcgState = rng;
            if (temp != 0) {
                temp = temp >> 28;
            } else {
                temp = 1;
            }
            mem->period = temp;
            if (arg0->spawnArg1 & 0x100000) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
            }
            if (arg0->spawnArg1 & 0x01000000) {
                gte_lddp(mem->scale << 2);
                vec = &mem->move;
                gte_ldsv(vec);
                gte_gpf12_real();
                gte_stsv(vec);
            }
            arg0->state = 1;
            if ((u16)mem->move.vx | (u16)mem->move.vy | (u16)mem->move.vz) {
                arg0->state = 2;
            }
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x43C0;
        prim->code |= 3;
        prim->u0    = (mem->age / mem->period) << 5;
        prim->v0    = 0x40;
        prim->u1    = ((mem->age / mem->period) << 5) + 0x1F;
        prim->v1    = 0x40;
        prim->u2    = (mem->age / mem->period) << 5;
        prim->v2    = 0x5F;
        prim->u3    = ((mem->age / mem->period) << 5) + 0x1F;
        prim->v3    = 0x5F;
        block->dx   = (((mem->scale * 31) / block->otz) * rsin(mem->angle)) >> 12;
        block->dy   = (((mem->scale * 31) / block->otz) * rcos(mem->angle)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = (((mem->scale * 31) / block->otz) * rsin(mem->angle + 0x400)) >> 12;
        block->dy   = (((mem->scale * 31) / block->otz) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
    if (Gp_State1C->eventState < 4) {
        if (D_801153F4 == 1) {
            return;
        }
        if (arg0->state == 2) {
            coord->coord.t[0] += mem->move.vx;
            coord->coord.t[1] += mem->move.vy;
            t2                 = coord->coord.t[2] + mem->move.vz;
            coord->flg         = 0;
            coord->coord.t[2]  = t2;
            mem->move.vy      += 3;
        }
        mem->age++;
        if (mem->age <= mem->period * 6 - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_dryfield_toilet_8017E64C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x6028A;
        D_80115730  = 0x6028B;
        D_80115754  = 0x6028C;
        arg0->state = 1;
    }
}

void func_dryfield_toilet_8017E69C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;
    MATRIX*        mtx;
    u8             col[4];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->sub                   = mem->parent;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = mem->pos.vx;
            coord->coord.t[1]            = mem->pos.vy;
            coord->coord.t[2]            = mem->pos.vz;
            coord->flg                   = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &((TmdObject*)player->extra)->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][2];
            func_dryfield_toilet_8017F4C0(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][2];
            func_dryfield_toilet_8017F4C0(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_dryfield_toilet_8017F4C0(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][2];
            func_dryfield_toilet_8017F4C0(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = (u16)mem->scale >> 1;
            col[2] = (u16)mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_dryfield_toilet_8017F09C(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}
