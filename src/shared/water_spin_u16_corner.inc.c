/* Part of the water effects library; see water_effects.h. */

/// Stores the rotated pixel offset from a water sprite's centre to one corner.
///
/// `projection` must point to one live, word-aligned `EffectShapeScratch` with
/// positive `depth` in SZ3/4 units, including any caller bias. `radiusScale`
/// is a signed 16-bit size parameter, even when a drawer receives it as s32.
/// Its projected half-diagonal is `radiusScale * 31 / depth` integer pixels,
/// truncated toward zero before multiplying by the Q12 sine and cosine.
/// Each product must fit s32; shifting to integer pixels rounds down.
///
/// `cornerAngle` uses 4096 units per turn and stays 32-bit across the caller's
/// quarter-turn addition. With positive scale, zero points up and a quarter
/// turn points right: X is rightward and Y upward, so the caller subtracts Y
/// from screen Y. Opposite corners use opposite signs of the same offset.
/// Only the two s32 components of `extent.corner` are overwritten; all other
/// fields are preserved. No allocation is made and no pointer is retained.
static inline void _waterComputeSpriteCornerOffset(EffectShapeScratch* projection, s16 radiusScale, s32 cornerAngle)
{
    enum {
        WATER_SPRITE_CORNER_PERSPECTIVE_SCALE  = 31,
        WATER_SPRITE_CORNER_TRIG_FRACTION_BITS = 12
    };
    s32    halfDiagonalPixels;
    q19_12 angleSine;
    q19_12 angleCosine;

    angleSine                   = rsin(cornerAngle);
    halfDiagonalPixels          = (radiusScale * WATER_SPRITE_CORNER_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.x = (halfDiagonalPixels * angleSine) >> WATER_SPRITE_CORNER_TRIG_FRACTION_BITS;
    angleCosine                 = rcos(cornerAngle);
    halfDiagonalPixels          = (radiusScale * WATER_SPRITE_CORNER_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.y = (halfDiagonalPixels * angleCosine) >> WATER_SPRITE_CORNER_TRIG_FRACTION_BITS;
}
