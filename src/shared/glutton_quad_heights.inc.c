/* Part of the Glutton library; see glutton.h. */

/// Sets two consecutive live grid quads to the Glutton's fixed wall heights.
///
/// Requires writable vertices `4 * firstQuadIndex` through
/// `4 * firstQuadIndex + 7` in the active grid, with a nonnegative quad index.
/// The first two vertices of each quad take Y = 500 and the last two Y = 800,
/// in world units; X/Z, face indices and normals are untouched. The first
/// argument is unused. Neither carrier calls this retained helper.
static void _gluttonSetQuadPairHeights(s32 unused, s16 firstQuadIndex)
{
    enum { GLUTTON_QUAD_PAIR_TOP_Y    = 500,
           GLUTTON_QUAD_PAIR_BOTTOM_Y = 800 };
    SVECTOR* vertices;

    vertices                            = Gp_GridParams->vertices;
    vertices[firstQuadIndex * 4].vy     = GLUTTON_QUAD_PAIR_TOP_Y;
    vertices[firstQuadIndex * 4 + 1].vy = GLUTTON_QUAD_PAIR_TOP_Y;
    vertices[firstQuadIndex * 4 + 2].vy = GLUTTON_QUAD_PAIR_BOTTOM_Y;
    vertices[firstQuadIndex * 4 + 3].vy = GLUTTON_QUAD_PAIR_BOTTOM_Y;
    vertices[firstQuadIndex * 4 + 4].vy = GLUTTON_QUAD_PAIR_TOP_Y;
    vertices[firstQuadIndex * 4 + 5].vy = GLUTTON_QUAD_PAIR_TOP_Y;
    vertices[firstQuadIndex * 4 + 6].vy = GLUTTON_QUAD_PAIR_BOTTOM_Y;
    vertices[firstQuadIndex * 4 + 7].vy = GLUTTON_QUAD_PAIR_BOTTOM_Y;
}
