#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;
extern s16 D_dryfield_toilet_80181120[][3];

void func_dryfield_toilet_8017EE18(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_toilet_8017F09C(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_dryfield_toilet_8017F4C0(GpCoord* arg0, s32 arg1, u8* rgb);
void func_dryfield_toilet_8017FA00(GpCoord* coord, s16 size);
void func_dryfield_toilet_8017FF2C(GpCoord* arg0, s32 arg1);

void func_dryfield_toilet_8017DCF0(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    GpEffWork* spawned;

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState < 4) {
        if (Gp_StateF0.field_4 == 1) {
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
            if ((mem->pos.vx | mem->pos.vy | mem->pos.vz) == 0) {
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
            gte_gpf12();
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
    GpEffWork*            mem;
    GpCoord*              coord;
    void**                scratch;
    u8*                   head;
    OverlaySpriteScratch* block;
    POLY_FT4*             prim;
    s32                   rng;
    s32                   temp;
    SVECTOR*              vec;
    s32                   t2;
    u8*                   tmp;
    u16                   vx;
    u16                   vz;

    coord = arg0->extra.tmd->coords;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    vx      = (u16)coord->workm.t[0];
    tmp     = head - 0x18;
    SOFT_USE_REG(tmp); /* keeps the carve apart from `block`, so the head store takes the copy */
    block         = (OverlaySpriteScratch*)tmp;
    block->vec.vx = vx;
    block->vec.vy = (u16)coord->workm.t[1];
    vz            = (u16)coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((OverlaySpriteScratch*)(head - 0x18))->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((OverlaySpriteScratch*)(head - 0x18))->sxy);
    gte_stszotz(&block->otz);
    if (((OverlaySpriteScratch*)(head - 0x18))->otz >= 0x11) {
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
                gte_gpf12();
                gte_stsv(vec);
            }
            arg0->state = 1;
            if (mem->move.vx | mem->move.vy | mem->move.vz) {
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
        prim->x0    = (u16)block->sxy.vx + (u16)block->dx;
        prim->x3    = (u16)block->sxy.vx - (u16)block->dx;
        prim->y0    = (u16)block->sxy.vy - (u16)block->dy;
        prim->y3    = (u16)block->sxy.vy + (u16)block->dy;
        block->dx   = (((mem->scale * 31) / block->otz) * rsin(mem->angle + 0x400)) >> 12;
        block->dy   = (((mem->scale * 31) / block->otz) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1    = (u16)block->sxy.vx + (u16)block->dx;
        prim->x2    = (u16)block->sxy.vx - (u16)block->dx;
        prim->y1    = (u16)block->sxy.vy - (u16)block->dy;
        prim->y2    = (u16)block->sxy.vy + (u16)block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
    if (Gp_State1C->eventState < 4) {
        if (Gp_StateF0.field_4 == 1) {
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
    GpEffWork* mem;
    GpCoord*   coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
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
            coord->sub                       = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->flg                       = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
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
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
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

/// A spark that flies to another frame. The first tick takes the offset from
/// its own frame to the target frame the spawn argument names, in its own
/// axes, and keeps 0xCC/0x1000 of it as its step. Each later tick moves it by
/// that step and, every other tick, draws it as a textured square at the next
/// animation frame; it releases its work block after 20 ticks. It pauses while
/// the room's event state is set and releases the block when that state
/// reaches 4.
void func_dryfield_toilet_8017EBF4(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    GpCoord*   target;
    VECTOR     delta;

    work   = task->spawnArg2;
    coord  = task->extra.tmd->coords;
    target = (GpCoord*)task->spawnArg1;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += work->pos.vx;
                coord->coord.t[1] += work->pos.vy;
                coord->coord.t[2] += work->pos.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_dryfield_toilet_8017EE18(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
void func_dryfield_toilet_8017EE18(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            u0;
    s32            u1;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex                                     = arg1;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)arg0->workm.t[1];
    vz                                      = (u16)arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        u0          = t + 0x60;
        u1          = t + 0x77;
        SOFT_USE_REG(u1);
        prim->u0 = u0;
        prim->u2 = u0;
        prim->v2 = 0x17;
        prim->v3 = 0x17;
        sarg     = (s16)arg2;
        prim->u1 = u1;
        prim->u3 = u1;
        t        = sarg * 24;
        setRGB0(prim, arg3, arg3, arg3);
        prim->v0    = 0;
        prim->v1    = 0;
        block->step = (t - sarg) / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (u16)block->sy - (u16)block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (u16)block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows.
void func_dryfield_toilet_8017F09C(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
void func_dryfield_toilet_8017F4C0(GpCoord* arg0, s32 arg1, u8* rgb)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    u16            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                      = (u16)arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = (u16)block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = (u16)block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = (u16)block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = (u16)block->sx;
            prim->y2 = (u16)block->sy;
            ang2     = ang + 0x200;
            prim->x3 = (u16)block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = (u16)block->sy + ((block->step * rcos(ang2)) >> 12);
            ang      = ang2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// A glowing burst effect task. Each frame it draws a disc and the glow of
/// `func_dryfield_toilet_8017FA00` at a growing size; while its second level
/// lasts it also draws a widening ring that fades out behind them. Once that
/// level has run down the main level fades too, and the work block is
/// released when it is nearly gone. It pauses while the room's event state is
/// set and releases the block when that state reaches 4.
void func_dryfield_toilet_8017F854(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_dryfield_toilet_8017F4C0(coord, (s16)(step * 2), rgb);
        func_dryfield_toilet_8017FA00(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_dryfield_toilet_8017F09C(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at `coord`: two camera-facing textured squares, the inner one
/// of half-size `size` and the outer one of `size * 3 / 2`, both scaled by
/// depth, plus a flat quad on the ground beneath it when there is ground. It
/// also places the `Gp_RoomCoords[2]` light at the coordinate with a randomly
/// flickering intensity. Nothing is drawn when the projection overflows.
void func_dryfield_toilet_8017FA00(GpCoord* coord, s16 size)
{
    GpCoord        ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = (s16)(shifted >> 0x11);
    light->head.b               = (s16)(shifted >> 0x12);
    light->head.u.at.local.t[0] = (s32)coord->coord.t[0];
    light->head.u.at.local.t[1] = (s32)coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    scratch                     = (void**)G_SCRATCH_HEAD;
    block                       = SCRATCH_HEAD_AT(scratch, GpRingScratch) - 1;
    block->vec.vx               = (u16)coord->workm.t[0];
    alias                       = block;
    vy                          = (u16)coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = (u16)coord->workm.t[2];
    Gp_LcgState = random;
    *scratch    = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = sc->sx - sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = sc->sx + sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = sc->sy - sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = sc->sy + sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = sc->sx - sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = sc->sx + sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = sc->sy - sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = sc->sy + sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_dryfield_toilet_8017FF2C(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues a semi-transparent textured quad lying flat at the world position of
/// `arg0`: the unit quad `D_80111E38` scaled by `arg1`, turned into view space
/// and projected. It is tinted `(0x30, 0x20, 0x20)` and alternates between two
/// texture frames on the display's animation frame counter. Nothing is drawn
/// when the projection overflows.
void func_dryfield_toilet_8017FF2C(GpCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        (u16) v->vx = (u16)v->vx + (u16)arg0->workm.t[0];
        tbl++;
        (u16) v->vy = (u16)v->vy + (u16)arg0->workm.t[1];
        i++;
        (u16) v->vz = (u16)v->vz + (u16)arg0->workm.t[2];
        v++;
    } while (i < 4);

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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = (u16)block->sxy0.vx;
        prim->y0    = (u16)block->sxy0.vy;
        prim->x1    = (u16)block->sxy1.vx;
        prim->y1    = (u16)block->sxy1.vy;
        prim->x2    = (u16)block->sxy2.vx;
        prim->y2    = (u16)block->sxy2.vy;
        prim->x3    = (u16)block->sxy3.vx;
        prim->y3    = (u16)block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}
