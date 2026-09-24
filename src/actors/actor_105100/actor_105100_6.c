#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"

/// Scratchpad block the ground quad is built in: the four corners in world
/// space, then their projected screen positions.
typedef struct Actor105100GroundScratch {
    SVECTOR vec[4];
    DVECTOR sxy0;
    DVECTOR sxy1;
    DVECTOR sxy2;
    DVECTOR sxy3;
} Actor105100GroundScratch;
STATIC_ASSERT_SIZEOF(Actor105100GroundScratch, 0x30);

/// Draws a flat textured quad on the ground under the actor: the corners of
/// the unit quad `D_80111E38`, scaled by `arg1` and turned into view
/// orientation, are placed around `arg0`'s world translation and projected.
/// When all four project, a semi-transparent `POLY_FT4` is queued one step
/// behind their depth, its texture alternating between two frames with the
/// display's animation frame.
void func_actor_105100_80132414(GsCOORDINATE2* arg0, s32 arg1)
{
    void**                    scratch;
    u8*                       head;
    Actor105100GroundScratch* sc;
    POLY_FT4*                 prim;
    GpQuadCorner*             tbl;
    SVECTOR*                  v;
    s32                       i;
    s32                       otz;
    s32                       flag;
    s32                       u;
    s32                       prod;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = (u8*)*scratch - sizeof(Actor105100GroundScratch);

    SOFT_TOUCH_REG(head);
    *scratch = head;
    sc       = (Actor105100GroundScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = sc->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy0);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_ldv3(&sc->vec[1], &sc->vec[2], &sc->vec[3]);
        gte_rtpt();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_stszotz(&otz);
            otz++;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);

            prim->r0    = 0x30;
            prim->g0    = 0x20;
            prim->b0    = 0x20;
            prim->tpage = 0x28;
            prim->clut  = 0x428C;
            setSemiTrans(prim, 1);
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
            prim->v0 = 0x38;
            prim->u0 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
            prim->v1 = 0x38;
            prim->u1 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
            prim->v2 = 0x57;
            prim->u2 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
            prim->v3 = 0x57;
            prim->u3 = u;
            prim->x0 = *(u16*)&sc->sxy0.vx;
            prim->y0 = *(u16*)&sc->sxy0.vy;
            prim->x1 = *(u16*)&sc->sxy1.vx;
            prim->y1 = *(u16*)&sc->sxy1.vy;
            prim->x2 = *(u16*)&sc->sxy2.vx;
            prim->y2 = *(u16*)&sc->sxy2.vy;
            prim->x3 = *(u16*)&sc->sxy3.vx;
            prim->y3 = *(u16*)&sc->sxy3.vy;
            addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor105100GroundScratch);
}
