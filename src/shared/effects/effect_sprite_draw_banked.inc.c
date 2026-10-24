/* Part of the effect sprite library; see effect_sprite.h. */

#ifndef EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW
/// Texture-page V origin of the banked sprite animation, measured in texel rows.
///
/// Supply an `int` constant before including this fragment to override 104.
/// Frames 0..11 use three 48-texel rows with inclusive V endpoints, so origins
/// 0..112 fit the GPU's byte coordinates without wrapping. The origin is added
/// in signed integer arithmetic before each V coordinate is narrowed to a byte.
/// Pod access tunnel and dumping hole use 104 (V 104..247); pod bottom and
/// garbage incinerator bind 112 (V 112..255). Palette selection is independent.
/// The fragment undefines the binding after the drawer definition.
#define EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW 104
#endif

#ifndef EFFECT_SPRITE_BANKED_INCREMENT_DEPTH
/// Enables a one-unit increment to the banked sprite's projected depth.
///
/// Bind the integer preprocessor constant 0 or 1 before including this fragment.
/// Value 0 keeps SZ3 >> 2; value 1 adds one after that division, keeping the
/// perspective-sizing divisor nonzero. The same depth feeds ordering-table
/// quantization, so the increment counts depth units rather than table entries.
/// The GTE's SZ3 register is unchanged. Pod bottom binds 1; pod access tunnel,
/// dumping hole and garbage incinerator use the default 0 and require nonzero
/// depth for accepted projections. This object-like switch has no arguments,
/// captures or runtime evaluation; the fragment undefines it after the drawer.
#define EFFECT_SPRITE_BANKED_INCREMENT_DEPTH 0
#elif (EFFECT_SPRITE_BANKED_INCREMENT_DEPTH != 0) && (EFFECT_SPRITE_BANKED_INCREMENT_DEPTH != 1)
#error "EFFECT_SPRITE_BANKED_INCREMENT_DEPTH must be 0 or 1"
#endif

/// Computes the signed pixel displacement to one corner of a depth-scaled sprite.
///
/// `scratch` borrows a live workspace whose `depth` is initialized and nonzero;
/// only `extent.corner` is replaced, and no pointer is retained. `sizeFactor`
/// scales the half-diagonal: `sizeFactor * 47 / depth` truncates toward zero
/// before rotation. The signed Q12 products are shifted to integer pixels,
/// rounding negative products down.
///
/// `cornerAngle` uses 4096 units per turn. The result's X points right and its
/// Y points up; for a positive half-diagonal, angle zero is upward and a quarter
/// turn is rightward. The drawer adds or subtracts these components to place
/// each pair of opposite corners.
static inline void _effectSpriteBankedRotateCorner(EffectShapeScratch* scratch, s16 sizeFactor, s32 cornerAngle)
{
    enum {
        /// Multiplier in the signed half-diagonal numerator, divided by projection depth.
        EFFECT_SPRITE_BANKED_PERSPECTIVE_SCALE = 47,
        /// Fractional bits in the signed `rsin` and `rcos` results; 4096 represents 1.0.
        ///
        /// Arithmetic right-shifting the rotated products by this count yields
        /// integer pixel offsets, rounding negative products down. Perspective
        /// division precedes multiplication by the trigonometric sample.
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

/// Draws a palette-selected animation frame as a rotating camera-facing quad.
///
/// `coord` is borrowed and must have its world transform composed. Its translation
/// is narrowed to signed 16-bit world units; its orientation does not turn the quad.
/// `frameAndPalette` packs the cell in bits 0..11 and the palette bank in bits
/// 12..15. Callers animate cells 0..11 of a five-column, 48-texel grid; the drawer
/// does not check that range. Banks 0 and 1 select per-cell palettes on VRAM rows
/// 270 and 271 (X = cell * 16 for these frames); banks 2..15 select X=240, Y=266.
/// The carrier selects the sheet's first texel row and optional depth increment above.
///
/// `size` is a signed perspective-sizing numerator, not a pixel half-extent:
/// `size * 47 / depth` is the screen-space half-diagonal in pixels, with integer
/// division before Q12 rotation. `depth` is SZ3 >> 2, optionally incremented by
/// one, and must be nonzero for an accepted projection. `angle` uses 4096 units per turn.
///
/// A nonnegative GTE FLAG emits one raw-texture, semi-transparent `POLY_FT4`
/// using additive blending on the 4-bit texture page at VRAM X=704, Y=0.
/// The shared primitive cursor must have room for that packet; no capacity check
/// occurs. One complete `EffectShapeScratch` block is borrowed from the initialized
/// scratch stack and released on every path; no coordinate or scratch pointer is
/// retained. Projection updates the GTE matrices and result registers.
static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    /// Left shift from a VRAM palette row to the GPU CLUT selector's row bits.
    ///
    /// As in `getClut`, bits 0..5 encode VRAM X in 16-word units; Y starts at bit 6.
    /// This drawer shifts only rows 270 and 271, fitting the 16-bit CLUT field.
    enum { EFFECT_SPRITE_BANKED_CLUT_ROW_SHIFT = 6 };

    enum {
        EFFECT_SPRITE_BANKED_FRAME_MASK        = 0xFFF,
        EFFECT_SPRITE_BANKED_PALETTE_SHIFT     = 12,
        EFFECT_SPRITE_BANKED_PALETTE_ROW_COUNT = 2,
        /// VRAM Y coordinate of bank 0's per-frame 16-colour palettes.
        ///
        /// Bank 1 uses the following row. Add the bank in row units before
        /// encoding the CLUT address; banks 2..15 use the fallback palette.
        EFFECT_SPRITE_BANKED_FIRST_PALETTE_ROW = 270,
        /// Selects the frame's six-bit VRAM X / 16 column in the GPU CLUT address.
        ///
        /// Columns span 16 VRAM words and wrap modulo 64 without changing the
        /// selected row. Only banks 0 and 1 use this field, below the encoded row.
        /// Texture-cell bounds are independent of this encoding.
        EFFECT_SPRITE_BANKED_CLUT_COLUMN_MASK = (1 << EFFECT_SPRITE_BANKED_CLUT_ROW_SHIFT) - 1,
        /// Row-major frame stride: five 48-texel cells across each texture-sheet row.
        ///
        /// The zero-based frame index advances across columns before rows.
        /// Palette selection and total animation length are independent.
        EFFECT_SPRITE_BANKED_CELLS_PER_ROW = 5,
        /// Square-cell side length and origin pitch along U and V, measured in texels.
        ///
        /// Each cell samples inclusive offsets 0..47, so its UV span is pitch minus one.
        /// This length is independent of palette selection and animation frame count.
        EFFECT_SPRITE_BANKED_CELL_PITCH_TEXELS = 48,
        /// First-to-last texel distance across one inclusive square animation cell.
        EFFECT_SPRITE_BANKED_UV_SPAN_TEXELS = EFFECT_SPRITE_BANKED_CELL_PITCH_TEXELS - 1,
        EFFECT_SPRITE_BANKED_QUARTER_TURN   = 0x400,
        EFFECT_SPRITE_BANKED_PACKET_CODE    = 0x2F // Textured quad, raw texture, semi-transparency
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
