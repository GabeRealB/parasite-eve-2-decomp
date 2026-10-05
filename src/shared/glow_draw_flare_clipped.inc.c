/* Part of the glow drawing library; see glow_draw.h. */

/// Sets a depth-clipped flare's square screen bounds from its projected centre.
///
/// Borrows a writable `POLY_FT4` and the projection for this call. `sx`, `sy`
/// and `radius` are in pixels; radius is the square's half-extent. Vertices
/// 0, 1, 2 and 3 become top-left, top-right, bottom-left and bottom-right,
/// respectively, with each coordinate narrowed to signed 16 bits. The caller
/// supplies the accepted projection, packet header, texture, colour and linkage.
static inline void _glowSetClippedFlareBounds(POLY_FT4* flare, const GlowCentreRadiusScratch* projection)
{
    flare->x0 = flare->x2 = projection->sx - projection->radius;
    flare->x1 = flare->x3 = projection->sx + projection->radius;
    flare->y0 = flare->y1 = projection->sy - projection->radius;
    flare->y2 = flare->y3 = projection->sy + projection->radius;
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
