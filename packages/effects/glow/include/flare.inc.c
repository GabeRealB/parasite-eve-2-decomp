/* Part of the glow drawing library; see glow.h. */

/// Sets a flare quad's corners around its projected center, `radius` pixels out.
static inline void _glowSetFlareBounds(POLY_FT4* flare, const GlowCentreScratch* projection)
{
    flare->x0 = flare->x2 = projection->sx - projection->radius;
    flare->x1 = flare->x3 = projection->sx + projection->radius;
    flare->y0 = flare->y1 = projection->sy - projection->radius;
    flare->y2 = flare->y3 = projection->sy + projection->radius;
}

/// Draws a flickering textured flare around a world point; `textureIndex` picks the sheet's cell and palette (0..2),
/// `radiusScale` its size in perspective. A point failing to project draws nothing.
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreScratch);
}
