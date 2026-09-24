#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "rooms/room_common.h"

/// `rtps` / `rtpt` / `rtv0`. The `inline_c.h` macros of those names assemble
/// to different words, so the instructions are spelled out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Draws a flickering flat quad at the coordinate `arg0`: scales the unit quad
/// `D_80111E38` by `arg1` in the x/z plane, rotates it by `Gfx_ViewWorldMtx`
/// with no translation, adds `arg0->workm.t`, and projects the four corners
/// through `GsWSMATRIX`. When the GTE flag is non-negative, queues one
/// semi-transparent `POLY_FT4` (tpage 0x28, clut 0x428C) coloured
/// `(0x30, 0x20, 0x20)`. Odd and even frames of `gDisplayState.animFrame`
/// alternate between two 32-texel columns (u 0xC0..0xDF or 0xE0..0xFF, v
/// 0x38..0x57).
void func_neo_ark_garden_80181020(GsCOORDINATE2* arg0, s32 arg1)
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
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
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
