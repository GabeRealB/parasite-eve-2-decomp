#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// `rtpt` / `mvmva 1,0,0,3,0` / `gpf 12`. The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32 Gp_LcgState;

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

void func_dryfield_night_motel_loft_8017E540(GsCOORDINATE2* coord, s16 scale, s16 shade);

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
    GsCOORDINATE2*                coord = ((TmdObject*)task->extra)->coords;
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
                    gte_gpf12_real();
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
                    gte_gpf12_real();
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
void func_dryfield_night_motel_loft_8017E540(GsCOORDINATE2* coord, s16 scale, s16 shade)
{
    _DryfieldNightMotelLoftTriScratch* blk;
    SVECTOR*                           p;
    SVECTOR*                           q;
    POLY_F3*                           prim;
    s32                                i;
    s32                                off;
    s32                                ang;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(_DryfieldNightMotelLoftTriScratch);
    blk                     = (_DryfieldNightMotelLoftTriScratch*)*(void**)G_SCRATCH_HEAD;
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
        gte_gpf12_real();
        gte_stsv((SVECTOR*)((u8*)blk + off));
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0((SVECTOR*)((u8*)blk + off));
        gte_mvmva_real();
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
        Gp_AddTpageShift((P_TAG*)prim, ((u32)Gp_LcgState >> 16) & 1, blk->otz);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(_DryfieldNightMotelLoftTriScratch);
}
