/* Part of the water effects library; see water_effects.h. */

/// Writes the signed pixel offset to a rotated water-sprite corner.
///
/// `projection` borrows a live scratch block with positive `depth` in SZ3/4
/// units (including the drawer's bias). Only `extent.corner` changes; no
/// pointer is retained. `radiusScale * 31 / depth` is the signed half-diagonal
/// in pixels, truncated toward zero. Q12 products must fit s32 and round down.
/// `angle` is a corner bearing in 4096 units per turn, with X right and Y up;
/// it stays 32-bit so a quarter-turn addition is not narrowed again.
static inline void _waterComputeSpriteCornerOffset(EffectShapeScratch* projection, s16 radiusScale, s32 angle)
{
    enum {
        WATER_SPIN_U16_PERSPECTIVE_SCALE  = 31,
        WATER_SPIN_U16_TRIG_FRACTION_BITS = 12
    };
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                  = rsin(angle);
    halfDiagonalPixels          = (radiusScale * WATER_SPIN_U16_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.x = (halfDiagonalPixels * trigSample) >> WATER_SPIN_U16_TRIG_FRACTION_BITS;
    trigSample                  = rcos(angle);
    halfDiagonalPixels          = (radiusScale * WATER_SPIN_U16_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.y = (halfDiagonalPixels * trigSample) >> WATER_SPIN_U16_TRIG_FRACTION_BITS;
}
