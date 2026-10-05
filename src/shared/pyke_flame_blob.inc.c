/* Part of the Pyke flame library; see pyke_flame.h. */

/// Copies an atlas frame's palette and inclusive UV corners into a flame quad.
static inline void _pykeFlameSetBlobTexture(POLY_FT4* quad, const EffectSpriteTextureFrame* textureFrame)
{
    quad->clut = getClut(textureFrame->clutX, textureFrame->clutY);
    quad->u0   = textureFrame->u;
    quad->v0   = textureFrame->v;
    quad->u1   = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->v1   = textureFrame->v;
    quad->u2   = textureFrame->u;
    quad->v2   = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->u3   = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->v3   = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
}

/// Draws the flying flame as a spinning, additive billboard at a world point.
///
/// Borrows the three s32 coordinates for this call and narrows each to s16.
/// `animationFrame` wraps modulo the twelve-frame `gEffectSpriteAtlasFrames`.
/// `sizeScale` is a sizing numerator: the screen half-diagonal in pixels is
/// sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN / (SZ3 / 4 + 1). `spinAngle` uses
/// 4096 units per turn. A negative GTE FLAG rejects the projection.
/// Reserves/releases one scratch block and appends one POLY_FT4 on acceptance;
/// the caller must provide scratch and frame-packet space through GPU drawing.
static void _pykeFlameDrawBlob(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale, s16 spinAngle)
{
    enum { QUARTER_TURN       = ONE / 4,
           TRIG_FRACTION_BITS = 12 };

    EffectShapeScratch*             scratchEnd;
    EffectShapeScratch*             scratch;
    EffectShapeScratch*             projectionScratch;
    POLY_FT4*                       quad;
    const EffectSpriteTextureFrame* textureFrame;
    u16                             textureFrameIndex;
    s32                             cornerAngle;
    u16                             worldZ;

    // Project the borrowed world point before allocating a GPU packet.
    scratchEnd                               = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (scratchEnd - 1)->worldPoint.vx          = (u16)worldPosition->vx;
    scratch                                  = scratchEnd - 1;
    scratch->worldPoint.vy                   = (u16)worldPosition->vy;
    worldZ                                   = (u16)worldPosition->vz;
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = scratch;
    scratch->worldPoint.vz                   = worldZ;
    projectionScratch                        = scratch;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    textureFrameIndex = animationFrame % ARRAY_SIZE(gEffectSpriteAtlasFrames);
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        scratch->depth = scratch->depth + 1;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage  = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
        textureFrame = &gEffectSpriteAtlasFrames[textureFrameIndex];
        _pykeFlameSetBlobTexture(quad, textureFrame);
        // Reuse the inclusive texel span to size the billboard's half-diagonal.
        cornerAngle              = spinAngle;
        scratch->extent.corner.x = (((sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN) / scratch->depth) * rsin(cornerAngle)) >> TRIG_FRACTION_BITS;
        scratch->extent.corner.y = (((sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN) / scratch->depth) * rcos(cornerAngle)) >> TRIG_FRACTION_BITS;
        quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;
        cornerAngle              = cornerAngle + QUARTER_TURN;
        quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;
        scratch->extent.corner.x = (((sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN) / scratch->depth) * rsin(cornerAngle)) >> TRIG_FRACTION_BITS;
        scratch->extent.corner.y = (((sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN) / scratch->depth) * rcos(cornerAngle)) >> TRIG_FRACTION_BITS;
        quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;
        quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
