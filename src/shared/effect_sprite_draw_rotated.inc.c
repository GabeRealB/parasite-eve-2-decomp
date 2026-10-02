/* Part of the effect sprite library; see effect_sprite.h. */

#ifndef EFFECT_SPRITE_ROTATED_DEPTH_BIAS
/// Adds one SZ3/4 unit before sizing and sorting when bound to 1.
///
/// Pod bottom supplies 1; the other carriers use the default 0. Bind the
/// integer preprocessor constant 0 or 1 before inclusion; cleared after use.
#define EFFECT_SPRITE_ROTATED_DEPTH_BIAS 0
#endif

/// Writes the signed pixel offset from a sprite's centre to one rotated corner.
///
/// `scratch` borrows a live workspace with positive `depth` already initialized;
/// only `extent.corner` is replaced and no pointer is retained. `sizeFactor`
/// gives a signed half-diagonal of `sizeFactor * 47 / depth` pixels, truncated
/// toward zero before rotation. Sizing and rotated products use signed 32-bit
/// arithmetic and must fit that width; no depth or overflow check occurs here.
///
/// `cornerAngle` uses 4096 units per turn. Q12 sine/cosine products are shifted
/// to integer pixels, rounding negative products down. X points right and Y
/// points up: a positive half-diagonal at angle zero points upward, and a
/// quarter turn points rightward. The drawer uses opposite signs for each pair
/// of corners and repeats this calculation a quarter turn later for the other pair.
static __inline__ void _effectSpriteRotatedComputeCornerOffset(EffectShapeScratch* scratch, s16 sizeFactor, s32 cornerAngle)
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

/// Draws a rotating camera-facing animation cell with a base or alternate palette.
///
/// `coord` is borrowed with `workm` already composed for projection (view space
/// in the drift tasks). Only its translation is used, narrowed to signed 16-bit
/// game-coordinate units. `frameAndPalette`
/// holds a frame in bits 0..11; any nonzero value in bits 12..15 selects the
/// alternate palette. Drift tasks animate frames 0..9 of a five-column grid
/// of 48-texel square cells, starting at U=0, V=128; no frame check occurs here.
/// The base 16-colour palette is at VRAM X=256, Y=271; the alternate is X=240,
/// Y=266. Both use the 4-bit texture page at VRAM X=768, Y=0.
///
/// `size` is a signed perspective numerator: `size * 47 / depth` gives the
/// screen-space half-diagonal in pixels before Q12 rotation. Division truncates
/// toward zero; the signed right shift rounds negative rotated products down.
/// `angle` uses 4096 units per turn. Depth is SZ3/4 plus the carrier's bias;
/// accepted projections require nonzero depth, without a check here.
///
/// A nonnegative GTE FLAG emits one raw-texture, additive semi-transparent
/// `POLY_FT4`. The primitive cursor must have room for the complete packet.
/// One `EffectShapeScratch` block is borrowed from the initialized scratch
/// stack and released on every path; no pointer is retained. Drawing updates
/// the GTE matrices and result registers.
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
#if EFFECT_SPRITE_ROTATED_DEPTH_BIAS
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef EFFECT_SPRITE_ROTATED_DEPTH_BIAS
