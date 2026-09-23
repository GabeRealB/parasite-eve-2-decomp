#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// Scratch block one band segment is projected in: the four corners in world
/// space, the GTE depth and flag, and the projected corners.
typedef struct _NeoArkObservatoryBandScratch {
    SVECTOR v[4];
    s32     otz;
    s32     flag;
    DVECTOR sxy[4];
} _NeoArkObservatoryBandScratch;

extern SVECTOR D_neo_ark_observatory_80181434[];
extern SVECTOR D_neo_ark_observatory_801814E4[];
extern SVECTOR D_neo_ark_observatory_801814F4[];
extern SVECTOR D_neo_ark_observatory_801814FC[];
extern SVECTOR D_neo_ark_observatory_8018150C[];
extern SVECTOR D_neo_ark_observatory_8018151C[];
extern SVECTOR D_neo_ark_observatory_80181524[];
extern SVECTOR D_neo_ark_observatory_80181564[];
extern SVECTOR D_neo_ark_observatory_80181574[];
extern SVECTOR D_neo_ark_observatory_8018157C[];
extern s16     D_neo_ark_observatory_80187A3C;

void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s16 arg3);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_4", func_neo_ark_observatory_8017FE34);

void func_neo_ark_observatory_80180124(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_neo_ark_observatory_80187A3C = 0;
        task->state                    = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw13(&D_neo_ark_observatory_801814E4[0], 0x280, 0x444);
            break;
        case 3: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814F4;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[1], 0x280, 0x444);
            break;
        }
        case 4:
        case 16: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814FC;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[1], 0x280, 0x444);
            Room_Draw13(&p[2], 0x280, 0x333);
            Room_Draw13(&p[3], 0x280, 0x222);
            break;
        }
        case 5:
        case 17: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018150C;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[1], 0x280, 0x444);
            break;
        }
        case 6:
        case 18:
            Room_Draw13(&D_neo_ark_observatory_8018151C[0], 0x200, 0x444);
            break;
        case 7: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181434;
            func_neo_ark_observatory_80180534(&p[0], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[2], 0x400, D_neo_ark_observatory_80187A3C, 0xC);
            func_neo_ark_observatory_80180534(&p[4], 0x400, D_neo_ark_observatory_80187A3C, 8);
        }
            /* fallthrough */
        case 19: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018151C;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[2], 0x200, 0x222);
            Room_Draw13(&p[6], 0x200, 0x222);
            Room_Draw13(&p[7], 0x200, 0x333);
            Room_Draw13(&p[8], 0x200, 0x444);
            Room_Draw13(&p[9], 0x200, 0x444);
            break;
        }
        case 8:
        case 20: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181564;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[1], 0x200, 0x444);
            Room_Draw13(&p[2], 0x200, 0x444);
            func_neo_ark_observatory_80180534(&p[-32], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-30], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-28], 0x400, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 9: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181574;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[1], 0x200, 0x444);
            Room_Draw13(&p[2], 0x200, 0x333);
            Room_Draw13(&p[3], 0x200, 0x222);
            Room_Draw13(&p[-6], 0x200, 0x222);
            Room_Draw13(&p[-8], 0x200, 0x333);
            func_neo_ark_observatory_80180534(&p[-28], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-26], 0x400, D_neo_ark_observatory_80187A3C, 0xC);
            func_neo_ark_observatory_80180534(&p[-24], 0x400, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 10: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181524;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[1], 0x200, 0x444);
            Room_Draw13(&p[5], 0x200, 0x444);
            Room_Draw13(&p[6], 0x200, 0x333);
            Room_Draw13(&p[7], 0x200, 0x222);
            func_neo_ark_observatory_80180534(&p[-12], 0x600, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 11: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018157C;
            Room_Draw13(&p[0], 0x200, 0x222);
            Room_Draw13(&p[1], 0x200, 0x333);
            Room_Draw13(&p[2], 0x200, 0x444);
            Room_Draw13(&p[-7], 0x200, 0x444);
            Room_Draw13(&p[-8], 0x200, 0x444);
            func_neo_ark_observatory_80180534(&p[-21], 0x600, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 12:
        case 14: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814E4;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[2], 0x280, 0x444);
            break;
        }
        case 21: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181564;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[1], 0x200, 0x444);
            break;
        }
    }
}

/// Draws a rotating ring of gouraud `POLY_G4` segments between two circles in
/// the XZ plane: an inner circle of radius `(s16)arg1 / 2` around `v[0]` and an
/// outer one of radius `(s16)arg1` around `v[1]`. `arg3` segments cover the
/// full turn, starting at a phase that advances with the frame counter. The
/// inner edge is lit at `arg2` plus a small pulse, fading to half at the far
/// corner and to black on the outer edge; nothing is drawn while that level
/// is negative.
void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s16 arg3)
{
    _NeoArkObservatoryBandScratch* blk;
    POLY_G4*                       prim;
    SVECTOR*                       outer;
    DisplayState*                  ds;
    s16                            start;
    s16                            step;
    s32                            angle;
    s32                            next;
    s16                            innerRadius;
    s16                            level;

    step        = 0x1000 / arg3;
    outer       = v + 1;
    innerRadius = (s16)arg1 >> 1;
    start       = gDisplayState.animFrame & 0xFFF;
    level       = arg2 + (rsin(gDisplayState.animFrame << 10) >> 10);
    if (level >= 0) {
        *(u8**)G_SCRATCH_HEAD -= sizeof(_NeoArkObservatoryBandScratch);
        blk                    = *(_NeoArkObservatoryBandScratch**)G_SCRATCH_HEAD;
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        for (angle = start; angle < start + step * arg3; angle = next) {
            blk->v[0].vx = v->vx + ((rsin(angle) * innerRadius) >> 12);
            blk->v[0].vy = v->vy;
            blk->v[0].vz = v->vz + ((rcos(angle) * innerRadius) >> 12);
            next         = angle + step;
            blk->v[1].vx = v->vx + ((rsin(next) * innerRadius) >> 12);
            blk->v[1].vy = v->vy;
            blk->v[1].vz = v->vz + ((rcos(next) * innerRadius) >> 12);
            blk->v[2].vx = outer->vx + ((rsin(angle) * (s16)arg1) >> 12);
            blk->v[2].vy = outer->vy;
            blk->v[2].vz = outer->vz + ((rcos(angle) * (s16)arg1) >> 12);
            blk->v[3].vx = outer->vx + ((rsin(next) * (s16)arg1) >> 12);
            blk->v[3].vy = outer->vy;
            blk->v[3].vz = outer->vz + ((rcos(next) * (s16)arg1) >> 12);
            gte_SetRotMatrix(&Gfx_ViewWorldMtx);
            gte_ldv0(&blk->v[0]);
            gte_rtps_real();
            gte_stsxy(&blk->sxy[0]);
            gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
            gte_rtpt_real();
            gte_stsxy3(&blk->sxy[1], &blk->sxy[2], &blk->sxy[3]);
            gte_stflg(&blk->flag);
            if (blk->flag >= 0) {
                gte_stszotz(&blk->otz);
                blk->otz++;
                ds             = &gDisplayState;
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, level, level, level);
                setRGB1(prim, level >> 1, level >> 1, level >> 1);
                setRGB2(prim, 0, 0, 0);
                setRGB3(prim, 0, 0, 0);
                addPrim((u_long*)((((u32)(blk->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        prim);
                prim->x0 = blk->sxy[0].vx;
                prim->y0 = blk->sxy[0].vy;
                prim->x1 = blk->sxy[1].vx;
                prim->y1 = blk->sxy[1].vy;
                prim->x2 = blk->sxy[2].vx;
                prim->y2 = blk->sxy[2].vy;
                prim->x3 = blk->sxy[3].vx;
                prim->y3 = blk->sxy[3].vy;
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(_NeoArkObservatoryBandScratch);
    }
}
