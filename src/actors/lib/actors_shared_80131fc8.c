#include "common.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "actors/actors_shared_80131fc8.h"

void ActorsShared80131fc8(s32 otz)
{
    u8*                head;
    u8*                allocated;
    ActorsDrawScratch* scratch;
    GpDrawAreaRec*     extra;
    DR_AREA*           area;
    DR_STP*            stp;
    DR_OFFSET*         off;
    SPRT*              sprt;
    DR_TPAGE*          tpage;
    TILE*              tile;
    RECT*              clip;
    u_short*           ofs;
    s32                val;
    s32                z;

    extra                   = Gp_GetViewSprtExtra();
    head                    = *(u8**)G_SCRATCH_HEAD;
    area                    = (DR_AREA*)Gpu_PrimCursor;
    allocated               = head - 0x14;
    *(void**)G_SCRATCH_HEAD = allocated;
    Gpu_PrimCursor          = (DR_TPAGE*)(area + 1);
    USE_REG(allocated);
    scratch      = (ActorsDrawScratch*)allocated;
    scratch->otz = otz;
    if (extra != NULL) {
        val = (extra->depth << gDisplayState.otDepthShift) & 0x3FFF;
        z   = otz;
        SOFT_TOUCH_REG(z);
        if ((val >> 4) < z) {
            scratch->rect   = extra->rect;
            scratch->rect.y = (u16)scratch->rect.y + gDisplayState.drawBuffer * 0x110;
        } else {
            goto block_4;
        }
    } else {
    block_4:
        scratch->rect.x = 0;
        scratch->rect.y = D_80070F87[0] * 0x110;
        scratch->rect.w = 0x140;
        scratch->rect.h = 0xF0;
    }
    clip = &scratch->rect;
    SetDrawArea(area, clip);
    addPrim(&Gpu_CurrentOt[scratch->otz], area);

    stp            = (DR_STP*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&Gpu_CurrentOt[scratch->otz], stp);

    ofs             = scratch->ofs;
    off             = (DR_OFFSET*)Gpu_PrimCursor;
    Gpu_PrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0xA0;
    scratch->ofs[1] = gDisplayState.drawBuffer * 0x110 + 0x78;
    SetDrawOffset(off, ofs);
    addPrim(&Gpu_CurrentOt[scratch->otz], off);

    sprt           = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = -0xA0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&Gpu_CurrentOt[scratch->otz], sprt);

    tpage          = Gpu_PrimCursor;
    Gpu_PrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
    addPrim(&Gpu_CurrentOt[scratch->otz], tpage);

    sprt           = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = 0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0x20;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&Gpu_CurrentOt[scratch->otz], sprt);

    tpage          = Gpu_PrimCursor;
    Gpu_PrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
    addPrim(&Gpu_CurrentOt[scratch->otz], tpage);

    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x60);
    tile->b0 = 2;
    tile->g0 = 2;
    tile->r0 = 2;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    addPrim(&Gpu_CurrentOt[scratch->otz], tile);

    stp            = (DR_STP*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&Gpu_CurrentOt[scratch->otz], stp);

    off             = (DR_OFFSET*)Gpu_PrimCursor;
    Gpu_PrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0x260;
    scratch->ofs[1] = 0x178;
    SetDrawOffset(off, ofs);
    addPrim(&Gpu_CurrentOt[scratch->otz], off);

    area            = (DR_AREA*)Gpu_PrimCursor;
    Gpu_PrimCursor  = (DR_TPAGE*)(area + 1);
    scratch->rect.x = 0x1C0;
    scratch->rect.y = 0x100;
    scratch->rect.w = 0x140;
    scratch->rect.h = 0xF0;
    SetDrawArea(area, clip);
    addPrim(&Gpu_CurrentOt[scratch->otz], area);

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}
