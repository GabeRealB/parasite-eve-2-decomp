/* Part of the frame capture library; see frame_capture.h. */

/// Scratch-stack workspace for queueing one frame capture.
///
/// `frameCaptureQueue` reserves one block, keeps the ordering-table index it
/// links every primitive at, and stages in it the arguments of the
/// drawing-environment setters. `SetDrawArea` and `SetDrawOffset` encode their
/// argument into the primitive at the call, so each of the two fields is
/// filled twice: first with the values that put drawing back on the current
/// draw buffer, then with those of the capture area. Nothing in the block
/// outlives the call.
///
/// The last word is never read or written, and neither setter reaches it. Its
/// role is unproven. Reserve the complete block and release it before
/// returning.
typedef struct {
    s32     otz;           // Ordering-table index every primitive of the capture is linked at
    u_short drawOffset[2]; // Drawing offset in VRAM pixels, x then y: the centre of the area being drawn to
    RECT    drawArea;      // Drawing (clip) area in VRAM pixels
    u32     field_10;      // Role unproven; the capture never reads or writes this word
} _FrameCaptureScratch;
STATIC_ASSERT_SIZEOF(_FrameCaptureScratch, 0x14);

/// Queues at depth `otz` a run of primitives that, executed in reverse of
/// the order they are added, point drawing at the 320x240 area at VRAM
/// (0x1C0, 0x100), fill it with a near-black tile with mask-bit setting on,
/// draw two 160x240 raw-texture sprites copied from the current draw buffer
/// over it, and then restore the draw offset, mask setting and draw area for
/// the current buffer. The first view draw-area rectangle is restored when
/// its scaled restore depth is below `otz`, full screen otherwise. The
/// `_FrameCaptureScratch` block staging the rectangle and offset is carved off
/// the scratch head and released before returning.
void frameCaptureQueue(s32 otz)
{
    _FrameCaptureScratch* scratch;
    SpriteDrawArea*       drawArea;
    DR_AREA*              area;
    DR_STP*               stp;
    DR_OFFSET*            off;
    SPRT*                 sprt;
    DR_TPAGE*             tpage;
    TILE*                 tile;
    RECT*                 clip;
    u_short*              ofs;

    drawArea       = Gp_GetViewSprtExtra();
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(_FrameCaptureScratch);
    scratch->otz   = otz;
    area           = gGpuPrimCursor;
    gGpuPrimCursor = area + 1;
    if (drawArea != NULL && ((drawArea->restoreDepth << gDisplayState.otDepthShift) & 0x3FFF) >> 4 < scratch->otz) {
        scratch->drawArea    = drawArea->clipRect;
        scratch->drawArea.y += gDisplayState.drawBuffer * 0x110;
    } else {
        scratch->drawArea.x = 0;
        scratch->drawArea.y = gDisplayState.drawBuffer * 0x110;
        scratch->drawArea.w = 0x140;
        scratch->drawArea.h = 0xF0;
    }
    clip = &scratch->drawArea;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    ofs                    = scratch->drawOffset;
    off                    = gGpuPrimCursor;
    gGpuPrimCursor         = off + 1;
    scratch->drawOffset[0] = 0xA0;
    scratch->drawOffset[1] = gDisplayState.drawBuffer * 0x110 + 0x78;
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

    off                    = gGpuPrimCursor;
    gGpuPrimCursor         = off + 1;
    scratch->drawOffset[0] = 0x260;
    scratch->drawOffset[1] = 0x178;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    area                = gGpuPrimCursor;
    gGpuPrimCursor      = area + 1;
    scratch->drawArea.x = 0x1C0;
    scratch->drawArea.y = 0x100;
    scratch->drawArea.w = 0x140;
    scratch->drawArea.h = 0xF0;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    SCRATCH_STACK_RELEASE_BLOCK(_FrameCaptureScratch);
}
