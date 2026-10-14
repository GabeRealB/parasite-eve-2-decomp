/* Part of the frame capture library; see frame_capture.h. */

/// Scratch-stack workspace for queueing one frame capture.
///
/// `FRAME_CAPTURE_QUEUE` reserves one block, keeps the ordering-table index it
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

/// Pixel geometry and packet encoding for the fixed game-frame capture layout.
enum {
    FRAME_CAPTURE_WIDTH_PIXELS           = 320,
    FRAME_CAPTURE_HEIGHT_PIXELS          = 240,
    FRAME_CAPTURE_BUFFER_Y_STRIDE        = 272,
    FRAME_CAPTURE_VRAM_X                 = 448,
    FRAME_CAPTURE_VRAM_Y                 = 256,
    FRAME_CAPTURE_SOURCE_PAGE_Y_SHIFT    = 8,
    FRAME_CAPTURE_RIGHT_SOURCE_PAGE_X    = 128,
    FRAME_CAPTURE_LOWER_BUFFER_TEXTURE_V = FRAME_CAPTURE_BUFFER_Y_STRIDE - (1 << FRAME_CAPTURE_SOURCE_PAGE_Y_SHIFT),
    FRAME_CAPTURE_DIRECT_COLOR           = 2,
    FRAME_CAPTURE_CLEAR_SHADE            = 2,
    FRAME_CAPTURE_DEPTH_TO_SLOT_SHIFT    = 4,
    FRAME_CAPTURE_DEPTH_MASK             = (s32)(((GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt)) << FRAME_CAPTURE_DEPTH_TO_SLOT_SHIFT) | ((1 << FRAME_CAPTURE_DEPTH_TO_SLOT_SHIFT) - 1)),
    FRAME_CAPTURE_SPRITE_PACKET_WORDS    = sizeof(SPRT) / sizeof(u_long) - 1,
    FRAME_CAPTURE_RAW_SPRITE_CODE        = 0x64 | SPRITE_SOURCE_RAW_TEXTURE,
};

void FRAME_CAPTURE_QUEUE(s32 orderingTableSlot)
{
    _FrameCaptureScratch* scratch;
    const SpriteDrawArea* viewDrawAreas;
    DR_AREA*              drawAreaPacket;
    DR_STP*               maskPacket;
    DR_OFFSET*            offsetPacket;
    SPRT*                 copySprite;
    DR_TPAGE*             texturePagePacket;
    TILE*                 clearTile;
    RECT*                 clipRect;
    u_short*              drawOffset;

    /// Queues one raw copy strip and its source-page command in this capture's slot.
    ///
    /// Arguments are side-effect-free pixel values: destination X relative to
    /// the center, source U relative to the page, and source-page VRAM X.
    /// Each occurs once. Captures `scratch`, `copySprite`, `texturePagePacket`,
    /// `gDisplayState`, `gGpuPrimCursor` and `gGpuCurrentOt`; consumes one SPRT
    /// and one DR_TPAGE, retained until drawing completes. Linkage prepends
    /// the page command so it executes before the sprite. No bounds checks.
#define FRAME_CAPTURE_QUEUE_COPY_STRIP(screenX, textureU, sourcePageX)                                                                                                                      \
    {                                                                                                                                                                                       \
        copySprite     = gGpuPrimCursor;                                                                                                                                                    \
        gGpuPrimCursor = copySprite + 1;                                                                                                                                                    \
        copySprite->x0 = (screenX);                                                                                                                                                         \
        copySprite->y0 = -FRAME_CAPTURE_HEIGHT_PIXELS / 2;                                                                                                                                  \
        copySprite->w  = FRAME_CAPTURE_WIDTH_PIXELS / 2;                                                                                                                                    \
        copySprite->h  = FRAME_CAPTURE_HEIGHT_PIXELS;                                                                                                                                       \
        copySprite->u0 = (textureU);                                                                                                                                                        \
        copySprite->v0 = gDisplayState.drawBuffer * FRAME_CAPTURE_LOWER_BUFFER_TEXTURE_V;                                                                                                   \
        setlen(copySprite, FRAME_CAPTURE_SPRITE_PACKET_WORDS);                                                                                                                              \
        setcode(copySprite, FRAME_CAPTURE_RAW_SPRITE_CODE);                                                                                                                                 \
        addPrim(&gGpuCurrentOt[scratch->otz], copySprite);                                                                                                                                  \
        texturePagePacket = gGpuPrimCursor;                                                                                                                                                 \
        gGpuPrimCursor    = texturePagePacket + 1;                                                                                                                                          \
        setDrawTPage(texturePagePacket, true, true, getTPage(FRAME_CAPTURE_DIRECT_COLOR, GPU_BLEND_AVERAGE, (sourcePageX), gDisplayState.drawBuffer << FRAME_CAPTURE_SOURCE_PAGE_Y_SHIFT)); \
        addPrim(&gGpuCurrentOt[scratch->otz], texturePagePacket);                                                                                                                           \
    }

    viewDrawAreas  = spriteGetViewDrawAreas();
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(_FrameCaptureScratch);
    scratch->otz   = orderingTableSlot;
    drawAreaPacket = gGpuPrimCursor;
    gGpuPrimCursor = drawAreaPacket + 1;

    // addPrim prepends: queue the state restored after the copy first.
    if (viewDrawAreas != NULL && ((viewDrawAreas->restoreDepth << gDisplayState.otDepthShift) & FRAME_CAPTURE_DEPTH_MASK) >> FRAME_CAPTURE_DEPTH_TO_SLOT_SHIFT < scratch->otz) {
        scratch->drawArea    = viewDrawAreas->clipRect;
        scratch->drawArea.y += gDisplayState.drawBuffer * FRAME_CAPTURE_BUFFER_Y_STRIDE;
    } else {
        scratch->drawArea.x = 0;
        scratch->drawArea.y = gDisplayState.drawBuffer * FRAME_CAPTURE_BUFFER_Y_STRIDE;
        scratch->drawArea.w = FRAME_CAPTURE_WIDTH_PIXELS;
        scratch->drawArea.h = FRAME_CAPTURE_HEIGHT_PIXELS;
    }
    clipRect = &scratch->drawArea;
    SetDrawArea(drawAreaPacket, clipRect);
    addPrim(&gGpuCurrentOt[scratch->otz], drawAreaPacket);

    maskPacket     = gGpuPrimCursor;
    gGpuPrimCursor = maskPacket + 1;
    SetDrawStp(maskPacket, false);
    addPrim(&gGpuCurrentOt[scratch->otz], maskPacket);

    drawOffset             = scratch->drawOffset;
    offsetPacket           = gGpuPrimCursor;
    gGpuPrimCursor         = offsetPacket + 1;
    scratch->drawOffset[0] = FRAME_CAPTURE_WIDTH_PIXELS / 2;
    scratch->drawOffset[1] = gDisplayState.drawBuffer * FRAME_CAPTURE_BUFFER_Y_STRIDE + FRAME_CAPTURE_HEIGHT_PIXELS / 2;
    SetDrawOffset(offsetPacket, drawOffset);
    addPrim(&gGpuCurrentOt[scratch->otz], offsetPacket);

    // Each 160-pixel half fits a 256-texel page; the right page starts at x=128.
    FRAME_CAPTURE_QUEUE_COPY_STRIP(-FRAME_CAPTURE_WIDTH_PIXELS / 2, 0, 0);
    FRAME_CAPTURE_QUEUE_COPY_STRIP(0, FRAME_CAPTURE_WIDTH_PIXELS / 2 - FRAME_CAPTURE_RIGHT_SOURCE_PAGE_X, FRAME_CAPTURE_RIGHT_SOURCE_PAGE_X);

    // Fill the capture with nonzero, masked texels before copying the scene.
    clearTile      = gGpuPrimCursor;
    gGpuPrimCursor = clearTile + 1;
    setTile(clearTile);
    clearTile->b0 = FRAME_CAPTURE_CLEAR_SHADE;
    clearTile->g0 = FRAME_CAPTURE_CLEAR_SHADE;
    clearTile->r0 = FRAME_CAPTURE_CLEAR_SHADE;
    clearTile->x0 = -FRAME_CAPTURE_WIDTH_PIXELS / 2;
    clearTile->y0 = -FRAME_CAPTURE_HEIGHT_PIXELS / 2;
    clearTile->w  = FRAME_CAPTURE_WIDTH_PIXELS;
    clearTile->h  = FRAME_CAPTURE_HEIGHT_PIXELS;
    addPrim(&gGpuCurrentOt[scratch->otz], clearTile);

    maskPacket     = gGpuPrimCursor;
    gGpuPrimCursor = maskPacket + 1;
    SetDrawStp(maskPacket, true);
    addPrim(&gGpuCurrentOt[scratch->otz], maskPacket);

    offsetPacket           = gGpuPrimCursor;
    gGpuPrimCursor         = offsetPacket + 1;
    scratch->drawOffset[0] = FRAME_CAPTURE_VRAM_X + FRAME_CAPTURE_WIDTH_PIXELS / 2;
    scratch->drawOffset[1] = FRAME_CAPTURE_VRAM_Y + FRAME_CAPTURE_HEIGHT_PIXELS / 2;
    SetDrawOffset(offsetPacket, drawOffset);
    addPrim(&gGpuCurrentOt[scratch->otz], offsetPacket);

    drawAreaPacket      = gGpuPrimCursor;
    gGpuPrimCursor      = drawAreaPacket + 1;
    scratch->drawArea.x = FRAME_CAPTURE_VRAM_X;
    scratch->drawArea.y = FRAME_CAPTURE_VRAM_Y;
    scratch->drawArea.w = FRAME_CAPTURE_WIDTH_PIXELS;
    scratch->drawArea.h = FRAME_CAPTURE_HEIGHT_PIXELS;
    SetDrawArea(drawAreaPacket, clipRect);
    addPrim(&gGpuCurrentOt[scratch->otz], drawAreaPacket);

    SCRATCH_STACK_RELEASE_BLOCK(_FrameCaptureScratch);
}

#undef FRAME_CAPTURE_QUEUE_COPY_STRIP
