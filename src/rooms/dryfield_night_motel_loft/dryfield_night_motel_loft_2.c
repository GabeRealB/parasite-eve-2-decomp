#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_night_motel_loft.h"
#include "rooms/room_common.h"

/// Scratch block one triangle is built in: the GTE depth and flag of its
/// projection, then its three corners in world space.
typedef struct _DryfieldNightMotelLoftTriScratch {
    s32     otz;
    s32     flag;
    SVECTOR v[3];
} _DryfieldNightMotelLoftTriScratch;

/// `Task::spawnArg2` block of a falling triangle: its velocity, the spin its
/// coordinate is rebuilt from each frame, the gain the velocity is scaled by
/// before it is applied, and the triangle's size and grey shade.
typedef struct _DryfieldNightMotelLoftShard {
    byte    unknown_0[0x10];
    SVECTOR vel;
    SVECTOR spin;
    byte    unknown_20[0x4];
    u16     gain;
    s16     size;
    s16     shade;
} _DryfieldNightMotelLoftShard;

/// The room's sprite points. The room draws them by view - 0 and 5 for views
/// 2 and 9, 1, 2 and 4 for 3 and 10, 2 for 4, 3 for 6, 4 and 5 for 7 and
/// 11 - and the two effect bursts write the seventh as the offset they spawn
/// at.
extern SVECTOR D_dryfield_night_motel_loft_8017ED78[];

/// The room's grid params: `8017ED54` is the template, `8017F120` the live
/// copy `func_dryfield_night_motel_loft_8017D9BC` rebuilds from it.
extern GpGridParams D_dryfield_night_motel_loft_8017ED54;
extern GpGridParams D_dryfield_night_motel_loft_8017F120;

void func_dryfield_night_motel_loft_8017DE14(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_motel_loft_8017E540(GpCoord* coord, s16 scale, s16 shade);

/// Restores the live grid's first face normal, its face record and its four
/// face corners from the template, then raises the corners by 0xBB8 in Y when
/// `arg0` is set.
void func_dryfield_night_motel_loft_8017D9BC(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_dryfield_night_motel_loft_8017F120;
    src = &D_dryfield_night_motel_loft_8017ED54;

    for (i = 0; i < 1; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 4; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0xBB8;
    }
    d.vz = 0;

    for (i = 0; i < 4; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

/// Per-view room draw. Views 2 and 9, 4, 6 and 7 and 11 each queue one or two
/// of the room's points, and views 3, 10 and 8 additionally run a burst of
/// effect 0x601B0 at the room's seventh point: 0x20 steps from state 1 (to
/// state 2) and 0x30 from state 0 (to state 1). Every step rolls the room LCG
/// (`Gp_LcgState`) four times and builds the offset vector from the top bits of
/// each draw, the last draw's low six bits biased by 0x10 riding along as the
/// spawn argument.
void func_dryfield_night_motel_loft_8017DB64(Task* arg0)
{
    s32 i;

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 9:
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[0], 0, 0x300);
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[5], 0, 0x300);
            break;
        case 3:
        case 10: {
            SVECTOR* p = &D_dryfield_night_motel_loft_8017ED78[1];

            func_dryfield_night_motel_loft_8017DE14(&p[0], 0, 0x300);
            func_dryfield_night_motel_loft_8017DE14(&p[1], 0, 0x300);
            func_dryfield_night_motel_loft_8017DE14(&p[3], 0, 0x300);
            if (arg0->state == 1) {
                SVECTOR* pos;

                i   = 0;
                pos = &p[-1];
                do {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vx   = 0xFB8 - (((u32)Gp_LcgState >> 16) & 0x3FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vy   = (((u32)Gp_LcgState >> 16) & 0x1FF) - 0xD5C;
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vz   = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601B0, NULL, (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10, &pos[6]);
                    i++;
                } while (i < 0x20);
                arg0->state = 2;
            }
            break;
        }
        case 4:
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[2], 0, 0x300);
            break;
        case 6:
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[3], 0, 0x300);
            break;
        case 7:
        case 11:
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[4], 0, 0x300);
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[5], 0, 0x300);
            break;
        case 8:
            if (arg0->state == 0) {
                SVECTOR* pos;

                i   = 0;
                pos = &D_dryfield_night_motel_loft_8017ED78[0];
                do {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vx   = 0xFB8 - (((u32)Gp_LcgState >> 16) & 0x3FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vy   = (((u32)Gp_LcgState >> 16) & 0x1FF) - 0xD5C;
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vz   = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601B0, NULL, (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10, &pos[6]);
                    i++;
                } while (i < 0x30);
                arg0->state = 1;
            }
            break;
    }
}

/// Queues a flickering sprite at the world point `arg0`: a
/// semi-transparent `POLY_FT4` square centred on the point's projection, with
/// half-width `arg2 * 39 / otz`, textured from the 40-texel cell `arg1` of
/// tpage 0x2B and shaded 0x20 or 0x30 on alternate frames. Points closer than
/// OTZ 0x11 are skipped.
void func_dryfield_night_motel_loft_8017DE14(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0xC;
    block   = (RoomDraw25Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        ds          = &gDisplayState;
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw25Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sx - *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sx + *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sy - *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sy + *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw25Scratch*)(head - 0xC))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0xC);
}

/// Task driving one tumbling triangle, drawn each frame by
/// `func_dryfield_night_motel_loft_8017E540` at the task's model coordinate.
/// State 0 rolls a random velocity (downward in Y), gain, shade and spin, with
/// the size taken from `Task::spawnArg1`. Each later frame rebuilds the
/// coordinate's rotation from the spin, moves it by the velocity scaled by the
/// gain and draws it; gravity then adds to the Y velocity, unless the move
/// took the triangle below the floor (`t[1] > 0`), in which case the move is
/// undone and the velocity halved with Y reflected. The first bounce enters
/// state 2, where the shade also fades by 4 a frame and the task frees itself
/// once it drops below 5. The task idles while `Gp_State1C->eventState` is 2
/// or 3 and frees itself at 4 or more.
void func_dryfield_night_motel_loft_8017E090(Task* task)
{
    _DryfieldNightMotelLoftShard* w     = task->spawnArg2;
    s16                           ev    = Gp_State1C->eventState;
    GpCoord*                      coord = task->extra.tmd->coords;
    SVECTOR                       step;

    if (ev < 4) {
        if (ev < 2) {
            switch (task->state) {
                case 0:
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    w->vel.vx   = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    w->vel.vy   = ((u32)Gp_LcgState >> 16) & 0x7F;
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    w->vel.vz   = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    w->gain     = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                    w->size     = task->spawnArg1 & 0xFFF;
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    w->shade    = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x40;
                    VectorNormalSS(&w->vel, &w->vel);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    w->spin.vx  = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    w->spin.vy  = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    w->spin.vz  = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    coord->flg  = 0;
                    task->state = 1;
                    break;
                case 1:
                    Gfx_RotMatrixXYZ(&coord->coord, &w->spin, 0);
                    MatrixNormal(&coord->coord, &coord->coord);
                    gte_lddp(w->gain);
                    gte_ldsv(&w->vel);
                    gte_gpf12();
                    gte_stsv(&step);
                    coord->coord.t[0] += step.vx;
                    coord->coord.t[1] += step.vy;
                    coord->coord.t[2] += step.vz;
                    coord->flg         = 0;
                    func_dryfield_night_motel_loft_8017E540(coord, w->size, w->shade);
                    if (coord->coord.t[1] > 0) {
                        coord->coord.t[0] -= step.vx;
                        coord->coord.t[1] -= step.vy;
                        coord->coord.t[2] -= step.vz;
                        w->vel.vx          = w->vel.vx >> 1;
                        w->vel.vy          = -(w->vel.vy >> 1);
                        w->vel.vz          = w->vel.vz >> 1;
                        task->state        = 2;
                    } else {
                        w->vel.vy += 0x180;
                    }
                    break;
                case 2:
                    w->shade -= 4;
                    if (w->shade < 5) {
                        goto release;
                    }
                    Gfx_RotMatrixXYZ(&coord->coord, &w->spin, 0);
                    MatrixNormal(&coord->coord, &coord->coord);
                    gte_lddp(w->gain);
                    gte_ldsv(&w->vel);
                    gte_gpf12();
                    gte_stsv(&step);
                    coord->coord.t[0] += step.vx;
                    coord->coord.t[1] += step.vy;
                    coord->coord.t[2] += step.vz;
                    coord->flg         = 0;
                    func_dryfield_night_motel_loft_8017E540(coord, w->size, w->shade);
                    if (coord->coord.t[1] > 0) {
                        coord->coord.t[0] -= step.vx;
                        coord->coord.t[1] -= step.vy;
                        coord->coord.t[2] -= step.vz;
                        w->vel.vx          = w->vel.vx >> 1;
                        w->vel.vy          = -(w->vel.vy >> 1);
                        w->vel.vz          = w->vel.vz >> 1;
                    } else {
                        w->vel.vy += 0x180;
                    }
                    break;
            }
        }
    } else {
    release:
        Gp_ReleaseState1CMem(w, task);
    }
}

/// Draws one flat grey `POLY_F3` of shade `shade`: an equilateral triangle of
/// radius `scale` in `coord`'s local YZ plane, its corners 0x555 apart,
/// rotated by `coord`'s `workm` and moved by its translation before projection
/// through `GsWSMATRIX`. A triangle the GTE flags as failed is dropped. The
/// triangle is made semi-transparent with a blend mode drawn from the LCG.
void func_dryfield_night_motel_loft_8017E540(GpCoord* coord, s16 scale, s16 shade)
{
    _DryfieldNightMotelLoftTriScratch* blk;
    SVECTOR*                           p;
    SVECTOR*                           q;
    POLY_F3*                           prim;
    s32                                i;
    s32                                off;
    s32                                ang;

    SCRATCH_PUSH(_DryfieldNightMotelLoftTriScratch);
    blk = SCRATCH_HEAD(_DryfieldNightMotelLoftTriScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // `off` is the corner's byte offset in the block, initialised with the
    // other locals; indexing `v[]` instead lets loop strength reduction
    // materialise the offset later, at the end of the preheader.
    for (i = 0, off = 8, ang = 0, p = (SVECTOR*)blk; i < 3; i++) {
        p[1].vx = 0;
        p[1].vy = rsin(ang);
        p[1].vz = rcos(ang);
        gte_lddp(scale);
        gte_ldsv((SVECTOR*)((u8*)blk + off));
        gte_gpf12();
        gte_stsv((SVECTOR*)((u8*)blk + off));
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0((SVECTOR*)((u8*)blk + off));
        gte_rtv0();
        gte_stsv((SVECTOR*)((u8*)blk + off));
        off    += 8;
        p[1].vx = *(u16*)&p[1].vx + *(u16*)&coord->workm.t[0];
        q       = p + 1;
        q->vy   = *(u16*)&q->vy + *(u16*)&coord->workm.t[1];
        q->vz   = *(u16*)&q->vz + *(u16*)&coord->workm.t[2];
        p       = q;
        ang    += 0x555;
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&blk->v[0], &blk->v[1], &blk->v[2]);
    gte_rtpt();
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
        Gp_AddTpageShift((P_TAG*)prim, ((u32)Gp_LcgState >> 16) & 1, blk->otz);
    }
    SCRATCH_POP(_DryfieldNightMotelLoftTriScratch);
}
