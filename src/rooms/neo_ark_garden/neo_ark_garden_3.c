#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "rooms/neo_ark_garden.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

/// `rtps` / `rtpt` / `mvmva`. The `inline_c.h` macros of those names assemble
/// to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 D_80070F70;

void func_neo_ark_garden_8017F42C(SVECTOR* arg0)
{
    MATRIX         m;
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    MATRIX*        wm;
    POLY_FT4*      prim;

    Gfx_RotMatrixX(&m, D_80070F70 << 7, 1);
    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &m;
    tbl     = D_80111E38;
    head    = (u8*)*scratch - 0x38;
    SOFT_TOUCH_REG(head);
    block    = (GpQuadScratch*)head;
    v        = block->vec;
    *scratch = block;
    do {
        v->vx = 0;
        TOUCH_REG(v);
        v->vy = (s16)tbl->x * 250;
        TOUCH_REG(v);
        v->vz = (s16)tbl->y * 250;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->vx;
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->vy;
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->vz;
        v++;
    } while (i < 4);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2D);
        prim->tpage = 0xAC;
        prim->clut  = 0x43C0;
        prim->u0    = 0;
        prim->v0    = 0;
        prim->u1    = 0x3F;
        prim->v1    = 0;
        prim->u2    = 0;
        prim->v2    = 0x3F;
        prim->u3    = 0x3F;
        prim->v3    = 0x3F;
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_garden/neo_ark_garden_3", func_neo_ark_garden_8017F790);
