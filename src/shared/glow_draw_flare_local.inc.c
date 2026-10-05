/* Part of the glow drawing library; see glow_draw.h. */

/// Places an allocated flare quad around its projected centre and half-extent.
///
/// Preserves signed-halfword screen-coordinate narrowing. The scratch block is
/// borrowed for this call; no texture, colour or linkage fields are changed.
static inline void _glowSetLocalFlareBounds(POLY_FT4* prim, const RoomGlowSpriteScratch* block)
{
    s16 screenEdge;

    screenEdge = block->screenPos.vx - block->halfExtent;
    prim->x2   = screenEdge;
    prim->x0   = screenEdge;
    screenEdge = block->screenPos.vx + block->halfExtent;
    prim->x3   = screenEdge;
    prim->x1   = screenEdge;
    screenEdge = block->screenPos.vy - block->halfExtent;
    prim->y1   = screenEdge;
    prim->y0   = screenEdge;
    screenEdge = block->screenPos.vy + block->halfExtent;
    prim->y3   = screenEdge;
    prim->y2   = screenEdge;
}

/// Draws a depth-clipped textured flare at a coordinate's local point.
///
/// Rotates `localPoint` through the composed `coord->workm`, adds its world
/// translation, and narrows the world coordinates to signed 16 bits before
/// view projection. `textureIndex` uses its signed low halfword to select a
/// 40-texel column and the low six palette-offset bits on texture page 0x2B;
/// callers use 0..2. The signed low halfword of `radiusScale` gives a pixel
/// half-extent of `radiusScale * 39 / depth`, where depth is camera Z / 4.
/// Borrows both inputs for the call. Reserves one packet even when depth is
/// below 17; accepted points queue a semitransparent quad with RGB intensity
/// 32 or 48 on alternating frames. GTE flags do not gate this drawer.
static void _glowDrawFlareLocal(const GfxCoord* coord, const SVECTOR* localPoint, s32 textureIndex, s32 radiusScale)
{
    RoomGlowSpriteScratch* block;
    POLY_FT4*              prim;
    const DisplayState*    display;
    s32                    columnIndex;
    s32                    signedRadiusScale;
    s32                    leftU;
    s32                    rightU;
    s32                    animationFrame;
    s32                    intensity;

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    // Transform the local point into a signed-halfword world position.
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(localPoint);
    gte_rtv0();
    gte_stsv(&block->worldPos);
    block->worldPos.vx += coord->workm.t[0];
    block->worldPos.vy += coord->workm.t[1];
    block->worldPos.vz += coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();

    // Packet reservation precedes depth rejection, including for invisible points.
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= GLOW_MIN_DEPTH) {
        display           = &gDisplayState;
        animationFrame    = (u8)display->animFrame;
        columnIndex       = (s16)textureIndex;
        signedRadiusScale = (s16)radiusScale;
        prim->tpage       = GLOW_FLARE_TEXTURE_PAGE;
        prim->clut        = (columnIndex & GLOW_FLARE_PALETTE_OFFSET_MASK) | GLOW_FLARE_PALETTE_BASE;
        leftU             = columnIndex * GLOW_FLARE_CELL_STRIDE;
        rightU            = leftU + GLOW_FLARE_CELL_LAST_TEXEL;
        prim->u1          = rightU;
        prim->u3          = rightU;
        prim->u0          = leftU;
        prim->u2          = leftU;
        prim->v0          = 0;
        prim->v1          = 0;
        prim->v2          = GLOW_FLARE_CELL_LAST_TEXEL;
        prim->v3          = GLOW_FLARE_CELL_LAST_TEXEL;
        intensity         = (animationFrame & 1) << GLOW_BRIGHT_FLICKER_SHIFT;
        intensity        += GLOW_FLICKER_BASE_INTENSITY;
        setSemiTrans(prim, 1);
        prim->r0          = intensity;
        prim->g0          = intensity;
        prim->b0          = intensity;
        block->halfExtent = (signedRadiusScale * GLOW_FLARE_CELL_LAST_TEXEL) / block->otz;
        _glowSetLocalFlareBounds(prim, block);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << display->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}
