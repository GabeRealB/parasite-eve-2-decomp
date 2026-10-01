/* Part of the frame capture library; see frame_capture.h. */

/// Queues at depth `otz` a run of primitives that, executed in reverse of
/// the order they are added, point drawing at the 320x240 area at VRAM
/// (0x1C0, 0x100), fill it with a near-black tile with mask-bit setting on,
/// draw two 160x240 raw-texture sprites copied from the current draw buffer
/// over it, and then restore the draw offset, mask setting and draw area for
/// the current buffer. The first view draw-area rectangle is restored when
/// its scaled restore depth is below `otz`, full screen otherwise. The 0x14-byte
/// block holding the rectangle and offset is carved off the scratch head and
/// released before returning.
void frameCaptureQueue(s32 otz)
{
    ActorsDrawScratch* scratch;
    SpriteDrawArea*    drawArea;
    DR_AREA*           area;
    DR_STP*            stp;
    DR_OFFSET*         off;
    SPRT*              sprt;
    DR_TPAGE*          tpage;
    TILE*              tile;
    RECT*              clip;
    u_short*           ofs;

    drawArea       = Gp_GetViewSprtExtra();
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(ActorsDrawScratch);
    scratch->otz   = otz;
    area           = gGpuPrimCursor;
    gGpuPrimCursor = area + 1;
    if (drawArea != NULL && ((drawArea->restoreDepth << gDisplayState.otDepthShift) & 0x3FFF) >> 4 < scratch->otz) {
        scratch->rect    = drawArea->clipRect;
        scratch->rect.y += gDisplayState.drawBuffer * 0x110;
    } else {
        scratch->rect.x = 0;
        scratch->rect.y = gDisplayState.drawBuffer * 0x110;
        scratch->rect.w = 0x140;
        scratch->rect.h = 0xF0;
    }
    clip = &scratch->rect;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    ofs             = scratch->ofs;
    off             = gGpuPrimCursor;
    gGpuPrimCursor  = off + 1;
    scratch->ofs[0] = 0xA0;
    scratch->ofs[1] = gDisplayState.drawBuffer * 0x110 + 0x78;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    sprt           = gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
    sprt->x0       = -0xA0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    sprt           = gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
    sprt->x0       = 0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0x20;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setlen(tile, 3);
    setcode(tile, 0x60);
    tile->b0 = 2;
    tile->g0 = 2;
    tile->r0 = 2;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    addPrim(&gGpuCurrentOt[scratch->otz], tile);

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    off             = gGpuPrimCursor;
    gGpuPrimCursor  = off + 1;
    scratch->ofs[0] = 0x260;
    scratch->ofs[1] = 0x178;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    area            = gGpuPrimCursor;
    gGpuPrimCursor  = area + 1;
    scratch->rect.x = 0x1C0;
    scratch->rect.y = 0x100;
    scratch->rect.w = 0x140;
    scratch->rect.h = 0xF0;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    SCRATCH_STACK_RELEASE_BLOCK(ActorsDrawScratch);
}
