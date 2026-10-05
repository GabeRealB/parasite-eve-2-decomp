/* Part of the water effects library; see water_effects.h. */

/// Places the four screen corners of an upright water sprite.
///
/// `projection` supplies raw 16-bit centre coordinates and the signed pixel
/// half-width `screenExtent`. The top is r + (r >> 1) pixels above the centre
/// and the bottom (r >> 1) below it. Arithmetic shifts retain odd and negative
/// rounding; GPU stores keep the low 16 bits. Only `quad`'s coordinates change.
/// Both objects are borrowed live storage; no pointer is retained.
static inline void _waterSetUprightSpriteCorners(POLY_FT4* quad, const EffectCentreScratch* projection)
{
    quad->x0 = quad->x2 = projection->screenX - projection->screenExtent;
    quad->x1 = quad->x3 = projection->screenX + projection->screenExtent;
    quad->y0 = quad->y1 = projection->screenY - projection->screenExtent - (projection->screenExtent >> 1);
    quad->y2 = quad->y3 = projection->screenY + (projection->screenExtent >> 1);
}
