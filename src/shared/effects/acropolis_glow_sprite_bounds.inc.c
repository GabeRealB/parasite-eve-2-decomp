/* Part of the Akropolis glow library; see acropolis_glows.h. */

/// Positions a light glow quad as a square around its projected screen centre.
///
/// Borrows a writable `quad` and a separate readable `projection`; only
/// `screenPos` and the nonnegative `halfExtent` need initialization, in pixels.
/// Centre plus/minus half-extent must fit signed 32 bits. Each edge narrows to
/// signed 16 bits without clipping, before its paired corner stores. Vertex
/// indices 0..3 are top-left, top-right, bottom-left and bottom-right.
/// Writes only the eight coordinate halfwords; no GPU packets are queued.
static inline void _acropolisGlowSetSpriteBounds(POLY_FT4* quad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left     = projection->screenPos.vx - projection->halfExtent;
    quad->x2 = left;
    quad->x0 = left;
    right    = projection->screenPos.vx + projection->halfExtent;
    quad->x3 = right;
    quad->x1 = right;
    top      = projection->screenPos.vy - projection->halfExtent;
    quad->y1 = top;
    quad->y0 = top;
    bottom   = projection->screenPos.vy + projection->halfExtent;
    quad->y3 = bottom;
    quad->y2 = bottom;
}
