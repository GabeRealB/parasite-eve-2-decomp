/* Part of the water effects library; see water_effects.h. */

#include "water_spin_u16_corner.inc.c"

/// Draws one rotated, camera-facing cell of the eight-frame water-drift strip.
///
/// `coord` is borrowed with `workm` already composed in the input space of
/// `GsWSMATRIX`; drift tasks normally supply view-space translations. Each
/// translation keeps its low 16 bits as a signed GTE coordinate.
/// `textureColumn` is unsigned. Frame 0..7 selects a 32-texel square
/// at V=224..255; UVs wrap to bytes without a bounds check. The 4-bit texture
/// page is at VRAM (704, 0), with its 16-colour palette at (304, 271).
///
/// `radiusScale * 31 / depth` gives the signed half-diagonal in pixels
/// before rotation. Drift tasks supply scale 0..4095. `spinAngle` uses
/// 4096 units per turn. With positive scale, zero puts the first corner above
/// the centre, a quarter turn to its right. Signed division truncates toward zero; the Q12 products
/// round down and must fit s32. Accepted projections require positive SZ3/4
/// depth; there is no extra check.
///
/// A nonnegative GTE FLAG queues one raw-texture, additive semi-transparent
/// `POLY_FT4`; the primitive cursor must have space for the complete packet.
/// One word-aligned `EffectShapeScratch` is reserved on the initialized scratch
/// stack and released on every path. No pointer is retained; GTE state changes.
static void _waterDrawSpinU16(const GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle)
{
    enum {
        WATER_SPIN_U16_CELL_SHIFT      = 5,
        WATER_SPIN_U16_UV_SPAN_TEXELS  = 31,
        WATER_SPIN_U16_FIRST_TEXEL_ROW = 224,
        WATER_SPIN_U16_LAST_TEXEL_ROW  = 255,
        WATER_SPIN_U16_QUARTER_TURN    = 0x400,
        WATER_SPIN_U16_PACKET_CODE     = 0x2F
    };
    void**              scratchCursor;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    SVECTOR*            projectionPoint;
    s32                 cellU;
    s32                 cornerAngle;
    s32                 perpendicularAngle;
    u16                 centreZ;

    // Project the composed centre before constructing the screen-space quad.
    scratchCursor                   = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd                      = *scratchCursor;
    (scratchEnd - 1)->worldPoint.vx = (u16)coord->workm.t[0];
    projection                      = scratchEnd - 1;
    projection->worldPoint.vy       = (u16)coord->workm.t[1];
    centreZ                         = (u16)coord->workm.t[2];
    *scratchCursor                  = projection;
    projection->worldPoint.vz       = centreZ;
    projectionPoint                 = &projection->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(projectionPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        quad           = gGpuPrimCursor;
        cornerAngle    = spinAngle;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, WATER_SPIN_U16_PACKET_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(304, 271);
        cellU       = textureColumn << WATER_SPIN_U16_CELL_SHIFT;
        setUV4(quad, cellU, WATER_SPIN_U16_FIRST_TEXEL_ROW, cellU + WATER_SPIN_U16_UV_SPAN_TEXELS, WATER_SPIN_U16_FIRST_TEXEL_ROW, cellU, WATER_SPIN_U16_LAST_TEXEL_ROW, cellU + WATER_SPIN_U16_UV_SPAN_TEXELS, WATER_SPIN_U16_LAST_TEXEL_ROW);
        // Opposite corners share an offset; the second pair is a quarter turn away.
        _waterComputeSpriteCornerOffset(projection, radiusScale, cornerAngle);
        quad->x0           = projection->screenX + (u16)projection->extent.corner.x;
        quad->x3           = projection->screenX - (u16)projection->extent.corner.x;
        quad->y0           = projection->screenY - (u16)projection->extent.corner.y;
        quad->y3           = projection->screenY + (u16)projection->extent.corner.y;
        perpendicularAngle = cornerAngle + WATER_SPIN_U16_QUARTER_TURN;
        _waterComputeSpriteCornerOffset(projection, radiusScale, perpendicularAngle);
        quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
        quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
        quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
        quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_POP_AT(scratchCursor, EffectShapeScratch);
}
