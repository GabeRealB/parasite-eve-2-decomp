#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/shelter_b6_nursery.h"
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtpt` / `mvmva 1,0,0,3,0` / `gpf 12` / `rtps`. The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")

extern u32 Gp_LcgState;

/// Scratch block one triangle is built in: its three corners in world space,
/// then the GTE depth and flag of its projection.
typedef struct _ShelterB6NurseryTriScratch {
    SVECTOR v[3];
    s32     otz;
    s32     flag;
} _ShelterB6NurseryTriScratch;

void func_shelter_b6_nursery_801829E4(GsCOORDINATE2* coord, s16 scale, s16 shade);
void func_shelter_b6_nursery_80181EDC(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b6_nursery_80182330(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3);

void func_shelter_b6_nursery_80181314(Task* task)
{
    SVECTOR        step;
    SVECTOR        pos;
    SVECTOR        base;
    TmdObject*     obj;
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s16            eventState;

    obj   = task->extra;
    work  = task->spawnArg2;
    coord = obj->coords;
    if ((Gp_GetViewIndex() & 0xFF) != 0xC) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    {
        eventState = Gp_State1C->eventState;
        if (eventState >= 2) {
            if (eventState >= 4) {
                Gp_ReleaseState1CMem(work, task);
            }
        } else {
            Gp_UpdateCoord(coord);
            if (task->state == 0) {
                obj->flags       &= 0xFF7F;
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx = ((Gp_LcgState >> 16) & 0x3F) + 0x60;
                work->field_10.vy = 0;
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz = ((Gp_LcgState >> 16) & 0x3F) + 0x20;
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_24    = ((Gp_LcgState >> 16) & 0x3F) + 0x40;
                VectorNormalSS(&work->field_10, &work->field_10);
                work->field_1A = 0;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->field_18 = -((Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->field_1C = 0x40 - ((Gp_LcgState >> 16) & 0x7F);
                coord->flg     = 0;
                task->state++;
                return;
            }
            Gfx_RotMatrixXYZ(&coord->coord, (SVECTOR*)&work->field_18, 0);
            MatrixNormal(&coord->coord, &coord->coord);
            gte_lddp(work->field_24);
            gte_ldsv(&work->field_10);
            gte_gpf12_real();
            gte_stsv(&step);
            coord->coord.t[0] += step.vx;
            coord->coord.t[1] += step.vy;
            coord->coord.t[2] += step.vz;
            coord->flg         = 0;
            gte_SetRotMatrix(&Gfx_ViewWorldMtx);
            gte_ldv0(&step);
            gte_mvmva_real();
            gte_stsv(&pos);
            base.vx = coord->workm.t[0];
            base.vy = coord->workm.t[1];
            base.vz = coord->workm.t[2];
            pos.vx += base.vx;
            pos.vy += base.vy;
            pos.vz += base.vz;
            if (func_800DE7CC(&pos, &base, &pos, &base) == 1) {
                coord->coord.t[0] -= step.vx;
                coord->coord.t[1] -= step.vy;
                coord->coord.t[2] -= step.vz;
                work->field_10.vx  = (base.vx >> 1) + (work->field_10.vx >> 1);
                work->field_10.vy  = base.vy + (work->field_10.vy >> 1);
                work->field_10.vz  = (base.vz >> 1) + (work->field_10.vz >> 1);
                VectorNormalSS(&work->field_10, &work->field_10);
                work->field_24 = (s16)work->field_24 * 2 / 3;
                gte_lddp(work->field_24);
                gte_ldsv(&work->field_10);
                gte_gpf12_real();
                gte_stsv(&step);
                coord->coord.t[0] += step.vx;
                coord->coord.t[1] += step.vy;
                coord->coord.t[2] += step.vz;
            } else {
                work->field_10.vy += 0x180;
            }
            if (work->field_22 & 1) {
                if ((s16)work->field_22 > 0x40) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A4, coord, ((Gp_LcgState >> 16) & 0x10FF) + 0x02183300, NULL);
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A4, coord, ((Gp_LcgState >> 16) & 0x1000) + 0x82101300, NULL);
                }
            }
            work->field_22++;
        }
    }
}

void func_shelter_b6_nursery_80181820(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            step;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = task->spawnArg1 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 7;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            task->state    = task->spawnArg1 < 0 ? 2 : 1;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                switch ((task->spawnArg1 >> 24) & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                    case 6:
                        work->field_10.vy = 0;
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 7:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = ((u32)Gp_LcgState >> 16) & 0xFF;
                        gte_SetRotMatrix(&work->field_8->coord);
                        gte_ldv0(&work->field_10);
                        gte_mvmva_real();
                        gte_stsv(&work->field_10);
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12_real();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            break;
        case 1:
            func_shelter_b6_nursery_80181EDC(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += (s16)work->field_10.vx;
                coord->coord.t[1] += (s16)work->field_10.vy;
                coord->coord.t[2] += (s16)work->field_10.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->field_10.vy += (s16)work->field_22 / 10;
                } else {
                    work->field_10.vy -= 2;
                }
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b6_nursery_80182330(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += (s16)work->field_10.vx;
                coord->coord.t[1] += (s16)work->field_10.vy;
                coord->coord.t[2] += (s16)work->field_10.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->field_10.vy += (s16)work->field_22 / 10;
                } else {
                    work->field_10.vy -= 1;
                }
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 8) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

void func_shelter_b6_nursery_80181EDC(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw27Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    u16                vz;
    u16                col;
    u16                row;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw27Scratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                       = (RoomDraw27Scratch*)(head - 0x1C);
    block->vec.vy                               = *(u16*)&coord->workm.t[1];
    vz                                          = *(u16*)&coord->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw27Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw27Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw27Scratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim->tpage = 0x2B;
            prim->clut  = 0x4384;
            prim->code |= 3;
            col         = arg1 % 5;
            row         = arg1 / 5;
            u0          = col * 48;
            v0          = row * 48;
            setUV4(prim, u0, v0 + 0x28, u0 + 0x2F, v0 + 0x28, u0, v0 + 0x57, u0 + 0x2F, v0 + 0x57);
            ang       = arg3;
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
            ang2      = ang + 0x400;
            prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void func_shelter_b6_nursery_80182330(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw27Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    s32                u0;
    s32                u1;
    s32                ang;
    s32                ang2;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw27Scratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                       = (RoomDraw27Scratch*)(head - 0x1C);
    block->vec.vy                               = *(u16*)&coord->workm.t[1];
    vz                                          = *(u16*)&coord->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw27Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw27Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw27Scratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x2B;
            prim->clut  = 0x4385;
            u0          = (arg1 & 7) << 5;
            u1          = u0 + 0x1F;
            setUV4(prim, u0, 0x88, u1, 0x88, u0, 0xA7, u1, 0xA7);
            ang       = arg3;
            block->dx = (((arg2 * 31) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 31) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
            ang2      = ang + 0x400;
            prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
            block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void func_shelter_b6_nursery_80182730(Task* task)
{
    SVECTOR        step;
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s16            eventState;

    work       = task->spawnArg2;
    eventState = Gp_State1C->eventState;
    coord      = ((TmdObject*)task->extra)->coords;
    if (eventState >= 2) {
        if (eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        if (task->state == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_24    = ((Gp_LcgState >> 16) & 0x3F) + 0x40;
            work->field_26    = task->spawnArg1 & 0xFFF;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = ((Gp_LcgState >> 16) & 0x7F) + 0x40;
            VectorNormalSS(&work->field_10, &work->field_10);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_18 = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_1A = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_1C = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            coord->flg     = 0;
            task->state++;
            return;
        }
        Gfx_RotMatrixXYZ(&coord->coord, (SVECTOR*)&work->field_18, 0);
        MatrixNormal(&coord->coord, &coord->coord);
        gte_lddp(work->field_24);
        gte_ldsv(&work->field_10);
        gte_gpf12_real();
        gte_stsv(&step);
        coord->coord.t[0] += step.vx;
        coord->coord.t[1] += step.vy;
        coord->coord.t[2] += step.vz;
        coord->flg         = 0;
        func_shelter_b6_nursery_801829E4(coord, work->field_26, work->field_28);
        if (coord->coord.t[1] > 0) {
            Gp_ReleaseState1CMem(work, task);
        } else {
            work->field_10.vy += 0x180;
        }
    }
}

/// Draws one flat grey triangle at `coord`: three corners at 120-degree steps
/// on a circle of radius `scale` in the coordinate's YZ plane, transformed by
/// its world matrix, projected with `GsWSMATRIX` and linked into the ordering
/// table at the triangle's depth with shade `shade`.
void func_shelter_b6_nursery_801829E4(GsCOORDINATE2* coord, s16 scale, s16 shade)
{
    _ShelterB6NurseryTriScratch* blk;
    SVECTOR*                     p;
    POLY_F3*                     prim;
    s32                          i;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(_ShelterB6NurseryTriScratch);
    blk                     = (_ShelterB6NurseryTriScratch*)*(void**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 3; i++) {
        p     = &blk->v[i];
        p->vx = 0;
        p->vy = rsin(i * 0x555);
        p->vz = rcos(i * 0x555);
        gte_lddp(scale);
        gte_ldsv(p);
        gte_gpf12_real();
        gte_stsv(p);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(p);
        gte_mvmva_real();
        gte_stsv(p);
        p->vx = *(u16*)&p->vx + *(u16*)&coord->workm.t[0];
        p->vy = *(u16*)&p->vy + *(u16*)&coord->workm.t[1];
        p->vz = *(u16*)&p->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&blk->v[0], &blk->v[1], &blk->v[2]);
    gte_rtpt_real();
    prim           = (POLY_F3*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyF3(prim);
    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_AddTpageShift((P_TAG*)prim, (Gp_LcgState >> 16) & 1, blk->otz);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(_ShelterB6NurseryTriScratch);
}

void func_shelter_b6_nursery_80182D14(s16 arg0, s16 arg1)
{
    D_shelter_b6_nursery_801879F0.field_0 = arg0;
    D_shelter_b6_nursery_801879F0.field_2 = arg1;
}
