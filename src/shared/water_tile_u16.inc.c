/* Part of the water effects library; see water_effects.h. */

#include "water_tile_u16_corners.inc.c"

/// Draws one upright, camera-facing cell of the eight-frame water-spray grid.
///
/// `coord` is borrowed with `workm` already composed in the input space of
/// `GsWSMATRIX`; drift tasks normally supply view-space translations. Each
/// translation keeps its low 16 bits as a signed GTE coordinate. `textureCell`
/// keeps its low 16 bits. Its low three bits choose a 56-texel square from four
/// columns and two rows, starting at V=112. The 4-bit texture page is at VRAM
/// (704, 0), with its 16-colour palette at (288, 271).
///
/// `(s16)radiusScale * 55 / depth` gives the signed horizontal half-width `r`
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
static void _waterDrawTileU16(const GfxCoord* coord, s32 textureCell, s32 radiusScale)
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
        projection->screenExtent = ((s16)radiusScale * WATER_TILE_U16_PERSPECTIVE_SCALE) / projection->depth;
        _waterSetUprightSpriteCorners(quad, projection);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
