/* Part of the effect sprite library; see effect_sprite.h. */

#ifndef EFFECT_SPRITE_ROTATED_INCREMENT_DEPTH
/// 1 adds one to the projected depth before sizing and sorting (default 0).
#define EFFECT_SPRITE_ROTATED_INCREMENT_DEPTH 0
#elif (EFFECT_SPRITE_ROTATED_INCREMENT_DEPTH != 0) && (EFFECT_SPRITE_ROTATED_INCREMENT_DEPTH != 1)
#error "EFFECT_SPRITE_ROTATED_INCREMENT_DEPTH must be 0 or 1"
#endif

/// Sets the corner offset for one pair of opposite corners: `sizeFactor` times 47 over the depth, turned to
/// `cornerAngle`.
static inline void _effectSpriteRotatedComputeCornerOffset(EffectShapeScratch* scratch, s16 sizeFactor, s32 cornerAngle)
{
    enum {
        EFFECT_SPRITE_ROTATED_PERSPECTIVE_SCALE  = 47, // Multiplier in the depth-divided half-diagonal numerator
        EFFECT_SPRITE_ROTATED_TRIG_FRACTION_BITS = 12  // Fractional bits in rsin/rcos samples; 4096 represents 1.0
    };
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample               = rsin(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * EFFECT_SPRITE_ROTATED_PERSPECTIVE_SCALE) / scratch->depth;
    scratch->extent.corner.x = (halfDiagonalPixels * trigSample) >> EFFECT_SPRITE_ROTATED_TRIG_FRACTION_BITS;
    trigSample               = rcos(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * EFFECT_SPRITE_ROTATED_PERSPECTIVE_SCALE) / scratch->depth;
    scratch->extent.corner.y = (halfDiagonalPixels * trigSample) >> EFFECT_SPRITE_ROTATED_TRIG_FRACTION_BITS;
}

/// Draws a spinning additive animation cell at the coordinate's translation. `frameAndPalette`'s low 12 bits pick the
/// cell, in a grid of 48-texel cells five to a row from V=128, and anything above them the alternate palette. `size`
/// times 47 over the depth is the half-diagonal.
static void _effectSpriteDrawRotated(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        EFFECT_SPRITE_ROTATED_FRAME_MASK        = 0xFFF,
        EFFECT_SPRITE_ROTATED_PALETTE_SHIFT     = 12,
        EFFECT_SPRITE_ROTATED_CELLS_PER_ROW     = 5,
        EFFECT_SPRITE_ROTATED_CELL_PITCH_TEXELS = 48,
        EFFECT_SPRITE_ROTATED_UV_SPAN_TEXELS    = EFFECT_SPRITE_ROTATED_CELL_PITCH_TEXELS - 1,
        // The signed origin encodes V=128 when stored in a GPU byte field.
        EFFECT_SPRITE_ROTATED_FIRST_TEXEL_ROW = -128,
        EFFECT_SPRITE_ROTATED_QUARTER_TURN    = 0x400,
        EFFECT_SPRITE_ROTATED_PACKET_CODE     = 0x2F // Raw-texture, semi-transparent textured quad
    };
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    u16                 cellColumn;
    u16                 cellRow;
    s32                 cellU;
    s32                 cellV;
    s32                 perpendicularAngle;
    u16                 paletteSelector;
    u16                 frameIndex;

    paletteSelector = frameAndPalette >> EFFECT_SPRITE_ROTATED_PALETTE_SHIFT;
    frameIndex      = frameAndPalette & EFFECT_SPRITE_ROTATED_FRAME_MASK;
    // Project only the composed centre; the corners are constructed in screen space.
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
#if EFFECT_SPRITE_ROTATED_INCREMENT_DEPTH
        // Use the incremented depth for both perspective sizing and sorting.
        scratch->depth++;
#endif
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, EFFECT_SPRITE_ROTATED_PACKET_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 768, 0);
        quad->clut  = paletteSelector ? getClut(240, 266) : getClut(256, 271);
        cellColumn  = frameIndex % EFFECT_SPRITE_ROTATED_CELLS_PER_ROW;
        cellRow     = frameIndex / EFFECT_SPRITE_ROTATED_CELLS_PER_ROW;
        cellU       = cellColumn * EFFECT_SPRITE_ROTATED_CELL_PITCH_TEXELS;
        cellV       = cellRow * EFFECT_SPRITE_ROTATED_CELL_PITCH_TEXELS;
        // Inclusive UV endpoints narrow to bytes, preserving the signed-origin wrap.
        setUV4(quad,
               cellU, cellV + EFFECT_SPRITE_ROTATED_FIRST_TEXEL_ROW,
               cellU + EFFECT_SPRITE_ROTATED_UV_SPAN_TEXELS, cellV + EFFECT_SPRITE_ROTATED_FIRST_TEXEL_ROW,
               cellU, cellV + (EFFECT_SPRITE_ROTATED_FIRST_TEXEL_ROW + EFFECT_SPRITE_ROTATED_UV_SPAN_TEXELS),
               cellU + EFFECT_SPRITE_ROTATED_UV_SPAN_TEXELS, cellV + (EFFECT_SPRITE_ROTATED_FIRST_TEXEL_ROW + EFFECT_SPRITE_ROTATED_UV_SPAN_TEXELS));

        // Opposite corners share a radius; GPU halfword stores wrap screen coordinates.
        _effectSpriteRotatedComputeCornerOffset(scratch, size, angle);
        quad->x0           = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x3           = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y0           = scratch->screenY - (u16)scratch->extent.corner.y;
        quad->y3           = scratch->screenY + (u16)scratch->extent.corner.y;
        perpendicularAngle = angle + EFFECT_SPRITE_ROTATED_QUARTER_TURN;
        _effectSpriteRotatedComputeCornerOffset(scratch, size, perpendicularAngle);
        quad->x1 = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x2 = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y1 = scratch->screenY - (u16)scratch->extent.corner.y;
        quad->y2 = scratch->screenY + (u16)scratch->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef EFFECT_SPRITE_ROTATED_INCREMENT_DEPTH
