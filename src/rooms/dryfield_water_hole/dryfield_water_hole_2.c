#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

void func_dryfield_water_hole_8017F5D4(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_water_hole_8017F9C0(GpCoord* arg0, s32 arg1, s32 arg2);

/// Per-frame driver of a particle effect, drawn as the spinning sprite of
/// `func_dryfield_water_hole_8017F5D4` (state 1) or, when the spawn
/// argument's top nibble is set, the upright sprite of
/// `func_dryfield_water_hole_8017F9C0` (state 2). The first frame takes
/// the size from the argument's low 12 bits, a random spin angle, and the ticks
/// per animation frame from bits 12-15. Unless the work block already carries a
/// velocity it picks one by the kind in bits 24-27 - none, a random upward
/// burst, a random spray, a narrow upward jet, or the block's stored direction
/// - scaled to the speed in bits 16-23 (0x40 when zero). Every later tick
/// draws, moves the coordinate by the velocity with gravity pulling it down,
/// and releases the block after animation frame 7. While the room's event
/// state is non-zero it only draws, releasing the block from event state 4 on.
void func_dryfield_water_hole_8017F118(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_dryfield_water_hole_8017F5D4(coord, (u16)work->index, work->scale, work->angle);
            } else {
                func_dryfield_water_hole_8017F9C0(coord, (u16)work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_dryfield_water_hole_8017F5D4(coord, (u16)work->index, work->scale, work->angle);
            break;
        case 2:
            func_dryfield_water_hole_8017F9C0(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        work->move.vy     += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning sprite at the coordinate's world position, projected
/// through `GsWSMATRIX`. If the projection is valid, one semi-transparent
/// `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued, its texture the 32-texel
/// column `arg1` of the strip at v 0xE0..0xFF. Its corners sit at
/// `(s16)arg2 * 31 / otz` from the projected point, rotated by the angle
/// `arg3`. The work block lives on the scratchpad stack.
void func_dryfield_water_hole_8017F5D4(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch = (void**)G_SCRATCH_HEAD;
    TOUCH_REG_USE(arg2, scratch);
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (arg1 & 0xFFFF) << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws an upright sprite at the coordinate's world position, projected
/// through `GsWSMATRIX`. If the projection is valid, one semi-transparent
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued, its texture the 56-texel
/// cell `arg1 & 7` of a four-wide, two-row grid starting at v 0x70. The quad
/// is `2 * r` on a side with `r = (s16)arg2 * 55 / otz`, and the projected
/// point sits a quarter of the way up from its bottom edge. The work block
/// lives on the scratchpad stack.
void func_dryfield_water_hole_8017F9C0(GpCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    u32            cell;
    s32            u1;
    s32            vbase;
    s32            v0;
    s32            v1;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG_USE(arg2, scratch);
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)arg0->workm.t[1];
    vz                                      = (u16)arg0->workm.t[2];
    tex                                     = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        SOFT_BARRIER();
        cell  = (u16)tex;
        tex   = (cell & 3) * 0x38;
        vbase = ((cell & 7) >> 2) * 0x38;
        v0    = vbase + 0x70;
        SOFT_USE_REG(v0);
        u1       = tex + 0x37;
        v1       = vbase - 0x59;
        prim->v2 = v1;
        prim->v3 = v1;
        TOUCH_REG(u1);
        sarg        = (s16)arg2;
        prim->v0    = v0;
        prim->v1    = v0;
        t           = sarg * 0x38;
        prim->u0    = tex;
        prim->u1    = u1;
        prim->u2    = tex;
        prim->u3    = u1;
        block->step = (t - sarg) / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        v1          = ((u16)block->sy - (u16)block->step) - (block->step >> 1);
        xy          = v1;
        ds          = &gDisplayState;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (block->step >> 1);
        prim->y3    = xy;
        prim->y2    = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}
