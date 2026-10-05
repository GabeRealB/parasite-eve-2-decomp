/* Part of the water effects library; see water_effects.h. */

/// Places an upright water quad around its projected lower-quarter anchor.
///
/// `projection` supplies `screenX`/`screenY`, the raw 16-bit encodings of the
/// signed screen anchor, and `screenExtent`, the signed pixel half-width r.
/// The left/right offsets are -r/+r;
/// top/bottom offsets are -r-(r>>1) and r>>1. Positive even r puts the anchor
/// a quarter of the height above the bottom; odd r shortens the height by one
/// pixel. The arithmetic shift rounds negative r down, retaining reversed
/// corners for negative widths. The additions and subtractions must fit s32;
/// packet coordinates keep their low 16 bits, without clipping.
///
/// Borrow a writable complete `POLY_FT4` and a distinct live projection block
/// with those three fields initialized. Only the quad's eight coordinate
/// halfwords change; texture, colour and linkage stay intact. No pointer is
/// retained, and neither GTE state nor the scratch-stack cursor changes.
static inline void _waterSetUprightSpriteCorners(POLY_FT4* quad, const EffectCentreScratch* projection)
{
    quad->x0 = quad->x2 = projection->screenX - projection->screenExtent;
    quad->x1 = quad->x3 = projection->screenX + projection->screenExtent;
    quad->y0 = quad->y1 = projection->screenY - projection->screenExtent - (projection->screenExtent >> 1);
    quad->y2 = quad->y3 = projection->screenY + (projection->screenExtent >> 1);
}
