/* Part of the glow drawing library; see glow_draw.h. */

/// Places an allocated flare quad around its projected centre and half-extent.
///
/// Preserves signed-halfword screen-coordinate narrowing. The scratch block is
/// borrowed for this call; no texture, colour or linkage fields are changed.
static inline void _glowSetFlareBounds(POLY_FT4* prim, const GlowCentreScratch* block)
{
    s16 screenEdge;

    screenEdge = block->sx - (u16)block->radius;
    prim->x2   = screenEdge;
    prim->x0   = screenEdge;
    screenEdge = block->sx + (u16)block->radius;
    prim->x3   = screenEdge;
    prim->x1   = screenEdge;
    screenEdge = block->sy - (u16)block->radius;
    prim->y1   = screenEdge;
    prim->y0   = screenEdge;
    screenEdge = block->sy + (u16)block->radius;
    prim->y3   = screenEdge;
    prim->y2   = screenEdge;
}

/// Draws a flickering textured flare centred on a world point.
///
/// `textureIndex` uses its signed low halfword to select a 40-texel column
/// and the low six palette-offset bits on texture page 0x2B; callers use 0..2.
/// The signed low halfword of `radiusScale` gives a pixel half-extent of
/// `radiusScale * 39 / depth`, where depth is camera Z / 4 and must be nonzero.
/// Rejects negative GTE flags before reserving a packet. Borrows `worldPoint`
/// for the call and queues one semitransparent textured quad in the current
/// frame, with an RGB intensity of 32 or 48 on alternating frames.
static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale)
{
    GlowCentreScratch* block;
    POLY_FT4*          prim;
    s32                columnIndex;
    s32                intensity;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreScratch);

    // Project into the current view before deriving screen radii and sorting depths.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        columnIndex = (s16)textureIndex;
        intensity   = (((u8)gDisplayState.animFrame & 1) * (1 << GLOW_BRIGHT_FLICKER_SHIFT)) + GLOW_FLICKER_BASE_INTENSITY;
        prim->tpage = GLOW_FLARE_TEXTURE_PAGE;
        prim->clut  = (columnIndex & GLOW_FLARE_PALETTE_OFFSET_MASK) | GLOW_FLARE_PALETTE_BASE;
        setUVWH(prim, columnIndex * GLOW_FLARE_CELL_STRIDE, 0, GLOW_FLARE_CELL_LAST_TEXEL, GLOW_FLARE_CELL_LAST_TEXEL);
        setRGB0(prim, intensity, intensity, intensity);
        setSemiTrans(prim, 1);
        block->radius = ((s16)radiusScale * GLOW_FLARE_CELL_LAST_TEXEL) / block->otz;
        _glowSetFlareBounds(prim, block);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreScratch);
}
