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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK_AT(scratchCursor, EffectShapeScratch);
}

#include "water_tile_u16_corners.inc.c"

/// Draws one upright, camera-facing cell of the eight-frame water-spray grid.
///
/// `coord` is borrowed with `workm` already composed in the input space of
/// `GsWSMATRIX`; drift tasks normally supply view-space translations. Each
/// translation keeps its low 16 bits as a signed GTE coordinate. `textureCell`
/// is unsigned. Its low three bits choose a 56-texel square from four
/// columns and two rows, starting at V=112. The 4-bit texture page is at VRAM
/// (704, 0), with its 16-colour palette at (288, 271).
///
/// `radiusScale * 55 / depth` gives the signed horizontal half-width `r`
/// in pixels. Drift tasks supply scale 0..4095. Signed division truncates
/// toward zero. The top edge is centre Y - r - (r >> 1), the bottom centre Y +
/// (r >> 1), placing the centre about a quarter of the height above the bottom
/// edge. Accepted projections require positive SZ3/4 depth; there is no extra
/// check.
///
/// A nonnegative GTE FLAG queues one raw-texture, additive semi-transparent
/// `POLY_FT4`; the primitive cursor must have space for the complete packet.
/// One word-aligned `EffectCentreScratch` is reserved on the initialized
/// scratch stack and released on every path. No pointer is retained; GTE state
/// changes.
static void _waterDrawTileU16(const GfxCoord* coord, u16 textureCell, s16 radiusScale)
{
    enum {
        WATER_TILE_U16_COLUMNS           = 4,
        WATER_TILE_U16_CELLS             = 8,
        WATER_TILE_U16_ROW_SHIFT         = 2,
        WATER_TILE_U16_CELL_TEXELS       = 56,
        WATER_TILE_U16_UV_SPAN_TEXELS    = WATER_TILE_U16_CELL_TEXELS - 1,
        WATER_TILE_U16_FIRST_TEXEL_ROW   = 112,
        WATER_TILE_U16_PERSPECTIVE_SCALE = 55,
        WATER_TILE_U16_PACKET_CODE       = 0x2F
    };
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    u16                  frameIndex;
    u32                  cellIndex;
    s32                  rowTexelOffset;
    u8                   firstU;
    u8                   lastU;
    u8                   firstV;
    u8                   lastV;

    // Project the composed centre before constructing the screen-space quad.
    frameIndex                = textureCell;
    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, WATER_TILE_U16_PACKET_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(288, 271);
        // Wrap the animation frame over the eight-cell texture grid.
        cellIndex      = frameIndex;
        firstU         = (cellIndex & (WATER_TILE_U16_COLUMNS - 1)) * WATER_TILE_U16_CELL_TEXELS;
        rowTexelOffset = ((cellIndex & (WATER_TILE_U16_CELLS - 1)) >> WATER_TILE_U16_ROW_SHIFT) * WATER_TILE_U16_CELL_TEXELS;
        firstV         = rowTexelOffset + WATER_TILE_U16_FIRST_TEXEL_ROW;
        lastV          = rowTexelOffset + WATER_TILE_U16_FIRST_TEXEL_ROW + WATER_TILE_U16_UV_SPAN_TEXELS;
        lastU          = firstU + WATER_TILE_U16_UV_SPAN_TEXELS;
        setUV4(quad, firstU, firstV, lastU, firstV, firstU, lastV, lastU, lastV);
        projection->screenExtent = (radiusScale * WATER_TILE_U16_PERSPECTIVE_SCALE) / projection->depth;
        _waterSetUprightSpriteCorners(quad, projection);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
