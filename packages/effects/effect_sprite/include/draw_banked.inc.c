/* Part of the effect sprite library; see effect_sprite.h. */

#ifndef EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW
/// Texel row the banked sprite's cells start at (default 104).
#define EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW 104
#endif

#ifndef EFFECT_SPRITE_BANKED_INCREMENT_DEPTH
/// 1 adds one to the projected depth before sizing and sorting (default 0).
#define EFFECT_SPRITE_BANKED_INCREMENT_DEPTH 0
#elif (EFFECT_SPRITE_BANKED_INCREMENT_DEPTH != 0) && (EFFECT_SPRITE_BANKED_INCREMENT_DEPTH != 1)
#error "EFFECT_SPRITE_BANKED_INCREMENT_DEPTH must be 0 or 1"
#endif

/// Sets the corner offset for one pair of opposite corners: `sizeFactor` times 47 over the depth, turned to
/// `cornerAngle`.
static inline void _effectSpriteBankedRotateCorner(EffectShapeScratch* scratch, s16 sizeFactor, s32 cornerAngle)
{
    enum {
        EFFECT_SPRITE_BANKED_PERSPECTIVE_SCALE  = 47,
        EFFECT_SPRITE_BANKED_TRIG_FRACTION_BITS = 12
    };
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample               = rsin(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * EFFECT_SPRITE_BANKED_PERSPECTIVE_SCALE) / scratch->depth;
    scratch->extent.corner.x = (halfDiagonalPixels * trigSample) >> EFFECT_SPRITE_BANKED_TRIG_FRACTION_BITS;
    trigSample               = rcos(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * EFFECT_SPRITE_BANKED_PERSPECTIVE_SCALE) / scratch->depth;
    scratch->extent.corner.y = (halfDiagonalPixels * trigSample) >> EFFECT_SPRITE_BANKED_TRIG_FRACTION_BITS;
}

/// Draws a spinning additive animation cell at the coordinate's translation. `frameAndPalette`'s low 12 bits pick the
/// cell, in a grid of 48-texel cells five to a row, and its top four a palette bank: banks 0 and 1 give each cell its
/// own palette on VRAM row 270 or 271, the others one shared palette. `size` times 47 over the depth is the
/// half-diagonal.
static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    /// Shift of a palette's VRAM row in a CLUT word.
    enum { EFFECT_SPRITE_BANKED_CLUT_ROW_SHIFT = 6 };

    enum {
        EFFECT_SPRITE_BANKED_FRAME_MASK        = 0xFFF,
        EFFECT_SPRITE_BANKED_PALETTE_SHIFT     = 12,
        EFFECT_SPRITE_BANKED_PALETTE_ROW_COUNT = 2,
        /// VRAM row of bank 0's per-cell palettes; bank 1's is the next.
        EFFECT_SPRITE_BANKED_FIRST_PALETTE_ROW = 270,
        /// A per-cell palette's column, VRAM X / 16, is the cell's index.
        EFFECT_SPRITE_BANKED_CLUT_COLUMN_MASK  = (1 << EFFECT_SPRITE_BANKED_CLUT_ROW_SHIFT) - 1,
        EFFECT_SPRITE_BANKED_CELLS_PER_ROW     = 5,
        EFFECT_SPRITE_BANKED_CELL_PITCH_TEXELS = 48,
        EFFECT_SPRITE_BANKED_UV_SPAN_TEXELS    = EFFECT_SPRITE_BANKED_CELL_PITCH_TEXELS - 1,
        EFFECT_SPRITE_BANKED_QUARTER_TURN      = 0x400,
        EFFECT_SPRITE_BANKED_PACKET_CODE       = 0x2F // Textured quad, raw texture, semi-transparency
    };
    EffectShapeScratch* scratchTop;
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    u16                 cellColumn;
    u16                 cellRow;
    s32                 cellU;
    s32                 cellV;
    s32                 cornerAngle;
    s32                 perpendicularAngle;
    u16                 paletteBank;
    u32                 frameIndex;

    // Stage only the world centre; both pairs of corners are built in screen space.
    scratchTop                      = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (scratchTop - 1)->worldPoint.vx = coord->workm.t[0];
    SCRATCH_STACK_CURSOR(void)      = scratchTop - 1;
    scratch                         = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    scratch->worldPoint.vy          = coord->workm.t[1];
    scratch->worldPoint.vz          = coord->workm.t[2];
    frameIndex                      = frameAndPalette;
    frameIndex                     &= EFFECT_SPRITE_BANKED_FRAME_MASK;
    paletteBank                     = frameAndPalette >> EFFECT_SPRITE_BANKED_PALETTE_SHIFT;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchTop - 1)->screenX);
    gte_stflg(&(scratchTop - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchTop - 1)->depth);
#if EFFECT_SPRITE_BANKED_INCREMENT_DEPTH
        // Use the incremented depth for both perspective sizing and sorting.
        scratch->depth++;
#endif
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, EFFECT_SPRITE_BANKED_PACKET_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        if (paletteBank >= EFFECT_SPRITE_BANKED_PALETTE_ROW_COUNT) {
            quad->clut = getClut(240, 266);
        } else {
            quad->clut = ((paletteBank + EFFECT_SPRITE_BANKED_FIRST_PALETTE_ROW) << EFFECT_SPRITE_BANKED_CLUT_ROW_SHIFT) |
                         (frameIndex & EFFECT_SPRITE_BANKED_CLUT_COLUMN_MASK);
        }
        {
            u32 cellIndex = (u16)frameIndex;

            cellColumn = cellIndex % EFFECT_SPRITE_BANKED_CELLS_PER_ROW;
            cellRow    = cellIndex / EFFECT_SPRITE_BANKED_CELLS_PER_ROW;
        }
        cornerAngle = angle;
        cellU       = cellColumn * EFFECT_SPRITE_BANKED_CELL_PITCH_TEXELS;
        cellV       = cellRow * EFFECT_SPRITE_BANKED_CELL_PITCH_TEXELS;
        // UV endpoints are inclusive; GPU fields narrow the coordinates to bytes.
        setUV4(quad,
               cellU, cellV + EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW,
               cellU + EFFECT_SPRITE_BANKED_UV_SPAN_TEXELS, cellV + EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW,
               cellU, cellV + (EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW + EFFECT_SPRITE_BANKED_UV_SPAN_TEXELS),
               cellU + EFFECT_SPRITE_BANKED_UV_SPAN_TEXELS, cellV + (EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW + EFFECT_SPRITE_BANKED_UV_SPAN_TEXELS));

        // Opposite corners share a radius; GPU halfword stores retain wrapped coordinates.
        _effectSpriteBankedRotateCorner(scratch, size, cornerAngle);
        quad->x0           = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x3           = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y0           = scratch->screenY - (u16)scratch->extent.corner.y;
        quad->y3           = scratch->screenY + (u16)scratch->extent.corner.y;
        perpendicularAngle = cornerAngle + EFFECT_SPRITE_BANKED_QUARTER_TURN;
        _effectSpriteBankedRotateCorner(scratch, size, perpendicularAngle);
        quad->x1 = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x2 = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y1 = scratch->screenY - (u16)scratch->extent.corner.y;
        quad->y2 = scratch->screenY + (u16)scratch->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW
#undef EFFECT_SPRITE_BANKED_INCREMENT_DEPTH
