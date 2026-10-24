/* Part of the water effects library; see water_effects.h. */

enum {
    WATER_SPIN_CELL_SHIFT         = 5,
    WATER_SPIN_CELL_TEXELS        = 1 << WATER_SPIN_CELL_SHIFT,
    WATER_SPIN_UV_SPAN_TEXELS     = WATER_SPIN_CELL_TEXELS - 1,
    WATER_SPIN_FIRST_TEXEL_ROW    = 0xE0,
    WATER_SPIN_QUARTER_TURN       = 0x400,
    WATER_SPIN_TRIG_FRACTION_BITS = 12,
    WATER_SPIN_PACKET_CODE        = 0x2F // Raw-texture, semi-transparent textured quad
};

/// Writes the signed pixel offsets for one corner of a rotated water sprite.
///
/// `workspace` borrows a live scratch block with positive `depth` in SZ3 / 4
/// units. Only `extent.corner` changes; no pointer is retained. The signed
/// half-diagonal is `radiusScale * 31 / depth` pixels, truncated toward zero
/// before rotation. Products must fit s32; the Q12 shift rounds down.
///
/// `angle` is an absolute corner bearing in 4096 units per turn, with X right
/// and Y up. For a positive half-diagonal, zero points up and a quarter turn
/// points right. The caller subtracts Y from screen Y and uses opposite signs
/// for each corner pair, repeating a quarter turn later for the other pair.
/// The angle stays 32-bit so the caller's quarter-turn addition is not narrowed.
static inline void _waterSpinComputeCornerOffset(EffectShapeScratch* workspace, s16 radiusScale, s32 angle)
{
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                 = rsin(angle);
    halfDiagonalPixels         = (radiusScale * WATER_SPIN_UV_SPAN_TEXELS) / workspace->depth;
    workspace->extent.corner.x = (halfDiagonalPixels * trigSample) >> WATER_SPIN_TRIG_FRACTION_BITS;
    trigSample                 = rcos(angle);
    halfDiagonalPixels         = (radiusScale * WATER_SPIN_UV_SPAN_TEXELS) / workspace->depth;
    workspace->extent.corner.y = (halfDiagonalPixels * trigSample) >> WATER_SPIN_TRIG_FRACTION_BITS;
}

/// Draws one rotated, camera-facing cell of the water-drift animation.
///
/// `coord` is borrowed with `workm` already composed for projection; drift
/// tasks supply view-space translations. Translation components retain their
/// low 16 bits as signed GTE coordinates. `textureColumn` selects frame 0..7
/// of eight 32-texel square cells at U=0..255, V=224..255. UVs wrap to bytes,
/// with no bounds check. The 4-bit texture page is at VRAM (704, 0), and its
/// 16-colour palette is at (304, 271).
///
/// `radiusScale * 31 / depth` gives the signed half-diagonal in pixels before
/// rotation; drift tasks pass 0..4095. `spinAngle` uses 4096 units per turn;
/// the angle is supplied by the caller. Depth is SZ3/4; accepted projections must have
/// positive depth, without a separate check here.
///
/// A nonnegative GTE FLAG emits one raw-texture, additive semi-transparent
/// `POLY_FT4`. The primitive cursor must have space for the complete packet.
/// One `EffectShapeScratch` is reserved on the initialized scratch stack and
/// released on every path. No pointer is retained; GTE state is overwritten.
static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle)
{
    EffectShapeScratch* workspace;
    POLY_FT4*           quad;
    s32                 cellU;
    s32                 perpendicularAngle;

    // Project the composed centre; the quad's corners are built in screen space.
    workspace                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    workspace->worldPoint.vx = (u16)coord->workm.t[0];
    workspace->worldPoint.vy = (u16)coord->workm.t[1];
    workspace->worldPoint.vz = (u16)coord->workm.t[2];
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
        setcode(quad, WATER_SPIN_PACKET_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(304, 271);
        cellU       = textureColumn << WATER_SPIN_CELL_SHIFT;
        setUV4(quad, cellU, WATER_SPIN_FIRST_TEXEL_ROW,
               cellU + WATER_SPIN_UV_SPAN_TEXELS, WATER_SPIN_FIRST_TEXEL_ROW,
               cellU, WATER_SPIN_FIRST_TEXEL_ROW + WATER_SPIN_UV_SPAN_TEXELS,
               cellU + WATER_SPIN_UV_SPAN_TEXELS, WATER_SPIN_FIRST_TEXEL_ROW + WATER_SPIN_UV_SPAN_TEXELS);

        // Opposite corners share an offset; the other pair is a quarter turn away.
        _waterSpinComputeCornerOffset(workspace, radiusScale, spinAngle);
        quad->x0           = workspace->screenX + (u16)workspace->extent.corner.x;
        quad->x3           = workspace->screenX - (u16)workspace->extent.corner.x;
        quad->y0           = workspace->screenY - (u16)workspace->extent.corner.y;
        quad->y3           = workspace->screenY + (u16)workspace->extent.corner.y;
        perpendicularAngle = spinAngle + WATER_SPIN_QUARTER_TURN;
        _waterSpinComputeCornerOffset(workspace, radiusScale, perpendicularAngle);
        quad->x1 = workspace->screenX + (u16)workspace->extent.corner.x;
        quad->x2 = workspace->screenX - (u16)workspace->extent.corner.x;
        quad->y1 = workspace->screenY - (u16)workspace->extent.corner.y;
        quad->y2 = workspace->screenY + (u16)workspace->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(workspace->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Sets an upright water tile's screen corners with its anchor near the bottom.
///
/// `projection` supplies raw 16-bit GTE screen coordinates and `screenExtent`,
/// the signed horizontal half-width in pixels. For half-width r, the top edge
/// is r + (r >> 1) pixels above the anchor and the bottom (r >> 1) below it.
/// The arithmetic shift rounds down, including for odd or negative extents;
/// packet stores retain the low 16 bits of each resulting coordinate.
/// Both pointers borrow separate live objects; only `quad`'s coordinates change,
/// and neither pointer is retained.
static inline void _waterTileSetScreenCorners(POLY_FT4* quad, const EffectCentreScratch* projection)
{
    s16 leftX;
    s16 rightX;
    s16 topY;
    s16 bottomY;

    leftX    = projection->screenX - projection->screenExtent;
    quad->x0 = quad->x2 = leftX;
    rightX              = projection->screenX + projection->screenExtent;
    quad->x1 = quad->x3 = rightX;
    topY                = projection->screenY - projection->screenExtent - (projection->screenExtent >> 1);
    quad->y0 = quad->y1 = topY;
    bottomY             = projection->screenY + (projection->screenExtent >> 1);
    quad->y2 = quad->y3 = bottomY;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(workspace->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
