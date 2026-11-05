/* Part of the frame capture library; see frame_capture.h. */

/// The capture's scratch-stack block: its slot, and the drawing offset and area it stages for the setters.
typedef struct {
    s32     otz;           // Ordering-table slot every packet is linked at
    u_short drawOffset[2]; // x, y: the center of the area being drawn to
    RECT    drawArea;      // Drawing (clip) area in VRAM pixels
    u32     field_10;      // Unused
} _FrameCaptureScratch;

/// Where the capture goes in VRAM, and how its packets are encoded.
enum {
    FRAME_CAPTURE_VRAM_X                 = 448,
    FRAME_CAPTURE_VRAM_Y                 = 256,
    FRAME_CAPTURE_SOURCE_PAGE_Y_SHIFT    = 8,
    FRAME_CAPTURE_RIGHT_SOURCE_PAGE_X    = 128,
    FRAME_CAPTURE_LOWER_BUFFER_TEXTURE_V = DISPLAY_BUFFER_STRIDE_ROWS - (1 << FRAME_CAPTURE_SOURCE_PAGE_Y_SHIFT),
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

    /// Queues one half of the frame copy: a 160x240 sprite at `screenX` from the page at `sourcePageX`, U `textureU`.
#define FRAME_CAPTURE_QUEUE_COPY_STRIP(screenX, textureU, sourcePageX)                                                                                                                      \
    {                                                                                                                                                                                       \
        copySprite     = gGpuPrimCursor;                                                                                                                                                    \
        gGpuPrimCursor = copySprite + 1;                                                                                                                                                    \
        copySprite->x0 = (screenX);                                                                                                                                                         \
        copySprite->y0 = -DISPLAY_SCREEN_HEIGHT / 2;                                                                                                                                        \
        copySprite->w  = DISPLAY_SCREEN_WIDTH / 2;                                                                                                                                          \
        copySprite->h  = DISPLAY_SCREEN_HEIGHT;                                                                                                                                             \
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
        scratch->drawArea.y += gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
    } else {
        scratch->drawArea.x = 0;
        scratch->drawArea.y = gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
        scratch->drawArea.w = DISPLAY_SCREEN_WIDTH;
        scratch->drawArea.h = DISPLAY_SCREEN_HEIGHT;
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
    scratch->drawOffset[0] = DISPLAY_SCREEN_WIDTH / 2;
    scratch->drawOffset[1] = gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS + DISPLAY_SCREEN_HEIGHT / 2;
    SetDrawOffset(offsetPacket, drawOffset);
    addPrim(&gGpuCurrentOt[scratch->otz], offsetPacket);

    // Each 160-pixel half fits a 256-texel page; the right page starts at x=128.
    FRAME_CAPTURE_QUEUE_COPY_STRIP(-DISPLAY_SCREEN_WIDTH / 2, 0, 0);
    FRAME_CAPTURE_QUEUE_COPY_STRIP(0, DISPLAY_SCREEN_WIDTH / 2 - FRAME_CAPTURE_RIGHT_SOURCE_PAGE_X, FRAME_CAPTURE_RIGHT_SOURCE_PAGE_X);

    // Fill the capture with nonzero, masked texels before copying the scene.
    clearTile      = gGpuPrimCursor;
    gGpuPrimCursor = clearTile + 1;
    setTile(clearTile);
    clearTile->b0 = FRAME_CAPTURE_CLEAR_SHADE;
    clearTile->g0 = FRAME_CAPTURE_CLEAR_SHADE;
    clearTile->r0 = FRAME_CAPTURE_CLEAR_SHADE;
    clearTile->x0 = -DISPLAY_SCREEN_WIDTH / 2;
    clearTile->y0 = -DISPLAY_SCREEN_HEIGHT / 2;
    clearTile->w  = DISPLAY_SCREEN_WIDTH;
    clearTile->h  = DISPLAY_SCREEN_HEIGHT;
    addPrim(&gGpuCurrentOt[scratch->otz], clearTile);

    maskPacket     = gGpuPrimCursor;
    gGpuPrimCursor = maskPacket + 1;
    SetDrawStp(maskPacket, true);
    addPrim(&gGpuCurrentOt[scratch->otz], maskPacket);

    offsetPacket           = gGpuPrimCursor;
    gGpuPrimCursor         = offsetPacket + 1;
    scratch->drawOffset[0] = FRAME_CAPTURE_VRAM_X + DISPLAY_SCREEN_WIDTH / 2;
    scratch->drawOffset[1] = FRAME_CAPTURE_VRAM_Y + DISPLAY_SCREEN_HEIGHT / 2;
    SetDrawOffset(offsetPacket, drawOffset);
    addPrim(&gGpuCurrentOt[scratch->otz], offsetPacket);

    drawAreaPacket      = gGpuPrimCursor;
    gGpuPrimCursor      = drawAreaPacket + 1;
    scratch->drawArea.x = FRAME_CAPTURE_VRAM_X;
    scratch->drawArea.y = FRAME_CAPTURE_VRAM_Y;
    scratch->drawArea.w = DISPLAY_SCREEN_WIDTH;
    scratch->drawArea.h = DISPLAY_SCREEN_HEIGHT;
    SetDrawArea(drawAreaPacket, clipRect);
    addPrim(&gGpuCurrentOt[scratch->otz], drawAreaPacket);

    SCRATCH_STACK_RELEASE_BLOCK(_FrameCaptureScratch);
}

#undef FRAME_CAPTURE_QUEUE_COPY_STRIP
