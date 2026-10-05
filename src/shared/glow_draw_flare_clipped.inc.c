/* Part of the glow drawing library; see glow_draw.h. */

/// Places an allocated flare quad around its projected centre and half-extent.
///
/// Preserves signed-halfword screen-coordinate narrowing. The scratch block is
/// borrowed for this call; no texture, colour or linkage fields are changed.
static inline void _glowSetClippedFlareBounds(POLY_FT4* prim, const GlowCentreRadiusScratch* block)
{
    prim->x0 = prim->x2 = block->sx - block->radius;
    prim->x1 = prim->x3 = block->sx + block->radius;
    prim->y0 = prim->y1 = block->sy - block->radius;
    prim->y2 = prim->y3 = block->sy + block->radius;
}

void glowDrawFlareClipped(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale)
{
    GlowCentreRadiusScratch* block;
    POLY_FT4*                prim;
    s32                      leftU;
    s32                      intensity;
    s32                      columnIndex;
    u8                       animationFrame;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiusScratch);
    // Project into the current view before deriving screen radii and sorting depths.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    // Packet reservation precedes depth rejection, including for invisible points.
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz >= GLOW_MIN_DEPTH) {
        columnIndex    = (s16)textureIndex;
        animationFrame = gDisplayState.animFrame;
        prim->tpage    = GLOW_FLARE_TEXTURE_PAGE;
        prim->clut     = (columnIndex & GLOW_FLARE_PALETTE_OFFSET_MASK) | GLOW_FLARE_PALETTE_BASE;
        leftU          = columnIndex * GLOW_FLARE_CELL_STRIDE;
        setUV4(prim, leftU, 0, leftU + GLOW_FLARE_CELL_LAST_TEXEL, 0, leftU, GLOW_FLARE_CELL_LAST_TEXEL, leftU + GLOW_FLARE_CELL_LAST_TEXEL, GLOW_FLARE_CELL_LAST_TEXEL);
        intensity = ((animationFrame & 1) << GLOW_BRIGHT_FLICKER_SHIFT) + GLOW_FLICKER_BASE_INTENSITY;
        setRGB0(prim, intensity, intensity, intensity);
        setSemiTrans(prim, 1);
        block->radius = ((s16)radiusScale * GLOW_FLARE_CELL_LAST_TEXEL) / block->otz;
        _glowSetClippedFlareBounds(prim, block);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiusScratch);
}
