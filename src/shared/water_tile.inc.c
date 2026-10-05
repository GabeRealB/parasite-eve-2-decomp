/* Part of the water effects library; see water_effects.h. */

/// Places the water tile's corners around its projected lower-quarter anchor.
///
/// `workspace` supplies the raw screen-coordinate halves and signed pixel
/// half-extent. Shifts round down; packet coordinates retain their low 16 bits.
/// Both pointers are borrowed, and only the quad's screen coordinates change.
static inline void _waterTileSetScreenCorners(POLY_FT4* quad, const EffectCentreScratch* workspace)
{
    quad->x0 = quad->x2 = workspace->screenX - workspace->screenExtent;
    quad->x1 = quad->x3 = workspace->screenX + workspace->screenExtent;
    quad->y0 = quad->y1 = workspace->screenY - workspace->screenExtent - (workspace->screenExtent >> 1);
    quad->y2 = quad->y3 = workspace->screenY + (workspace->screenExtent >> 1);
}

/// Draws one upright, camera-facing tile of the water-drift animation.
///
/// `coord` borrows an already composed `workm`; drift tasks supply view-space
/// translations. Components narrow to signed 16-bit GTE coordinates before
/// projection through `GsWSMATRIX`. `frameIndex` selects 0..7 in a four-column,
/// two-row atlas of 56-texel cells starting at V=112. Nonnegative indices repeat
/// every eight frames; signed remainder and byte UV truncation are retained.
///
/// `radiusScale * 55 / depth` is the signed pixel half-width, truncated toward
/// zero; drift tasks supply 0..4095. Depth is SZ3/4 and must be positive for an
/// accepted projection. For half-extent r, vertical offsets are -r-(r>>1)
/// and r>>1: shifts round down, placing the anchor near the lower quarter.
/// Final screen coordinates retain their low 16 bits.
///
/// A nonnegative GTE FLAG queues one raw-texture, additive semi-transparent
/// `POLY_FT4`, using a 4-bit texture page at VRAM (704, 0) and palette (288, 271).
/// The primitive cursor needs space for one complete packet, which remains live
/// until the GPU consumes its ordering table. One `EffectCentreScratch` is
/// reserved on the initialized scratch stack and released on every path.
/// The coordinate and scratch pointers are not retained; GTE state is overwritten.
static void _waterDrawTile(const GfxCoord* coord, s16 frameIndex, s16 radiusScale)
{
    enum {
        WATER_TILE_COLUMNS         = 4,
        WATER_TILE_FRAME_COUNT     = 8,
        WATER_TILE_CELL_TEXELS     = 56,
        WATER_TILE_UV_SPAN_TEXELS  = WATER_TILE_CELL_TEXELS - 1,
        WATER_TILE_FIRST_TEXEL_ROW = 112,
        WATER_TILE_PACKET_CODE     = 0x2F // Raw-texture, semi-transparent textured quad
    };
    EffectCentreScratch* workspace;
    POLY_FT4*            quad;

    // Project the composed centre; the corners are constructed in screen space.
    workspace                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    workspace->worldPoint.vx = coord->workm.t[0];
    workspace->worldPoint.vy = coord->workm.t[1];
    workspace->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&workspace->worldPoint);
    gte_rtps();
    gte_stsxy(&workspace->screenX);
    gte_stflg(&workspace->projectionFlags);
    if (workspace->projectionFlags >= 0) {
        gte_stszotz(&workspace->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, WATER_TILE_PACKET_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(288, 271);
        setUVWH(quad, (frameIndex % WATER_TILE_COLUMNS) * WATER_TILE_CELL_TEXELS,
                (frameIndex % WATER_TILE_FRAME_COUNT) / WATER_TILE_COLUMNS * WATER_TILE_CELL_TEXELS + WATER_TILE_FIRST_TEXEL_ROW,
                WATER_TILE_UV_SPAN_TEXELS, WATER_TILE_UV_SPAN_TEXELS);

        // Keep the animation's anchor below the quad's geometric centre.
        workspace->screenExtent = (radiusScale * WATER_TILE_UV_SPAN_TEXELS) / workspace->depth;
        _waterTileSetScreenCorners(quad, workspace);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)workspace->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
