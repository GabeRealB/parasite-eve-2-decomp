#include "common.h"
#include "gameplay/3CD8.h"
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

/// `rtpt` / `mvmva 1,0,0,3,0` / `gpf 12`. The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern u32 Gp_LcgState;

/// Scratch block one triangle is built in: its three corners in world space,
/// then the GTE depth and flag of its projection.
typedef struct _ShelterB6NurseryTriScratch {
    SVECTOR v[3];
    s32     otz;
    s32     flag;
} _ShelterB6NurseryTriScratch;

void func_shelter_b6_nursery_801829E4(GsCOORDINATE2* coord, s16 scale, s16 shade);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80181314);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80181820);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80181EDC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80182330);

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
