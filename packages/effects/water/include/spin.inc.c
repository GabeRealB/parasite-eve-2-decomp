/* Part of the water effects library; see water.h. */

enum {
    WATER_SPIN_CELL_SHIFT         = 5,
    WATER_SPIN_CELL_TEXELS        = 1 << WATER_SPIN_CELL_SHIFT,
    WATER_SPIN_UV_SPAN_TEXELS     = WATER_SPIN_CELL_TEXELS - 1,
    WATER_SPIN_FIRST_TEXEL_ROW    = 0xE0,
    WATER_SPIN_QUARTER_TURN       = 0x400,
    WATER_SPIN_TRIG_FRACTION_BITS = 12,
    WATER_SPIN_PACKET_CODE        = 0x2F // Raw-texture, semi-transparent textured quad
};

/// Sets the pixel offset from a spin sprite's center to one corner at `angle` (0 up, a quarter turn right), sized by
/// `radiusScale` over the projected depth. Y points up, so the caller subtracts it from screen Y.
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

/// Draws an additive camera-facing quad at the coordinate, rotated by `spinAngle`, showing cell `textureColumn` (0..7)
/// of the drift animation; `radiusScale` sets its size before perspective.
static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle)
{
    EffectShapeScratch* workspace;
    POLY_FT4*           quad;
    s32                 cellU;
    s32                 perpendicularAngle;

    // Project the composed center; the quad's corners are built in screen space.
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

/// Sets an upright quad's corners `screenExtent` pixels to each side of its projected anchor, the anchor a quarter of
/// the height above the bottom edge.
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

/// Draws an additive upright camera-facing quad at the coordinate, showing cell `frameIndex` (0..7) of a 4x2 animation
/// atlas; `radiusScale` sets its size before perspective.
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

    // Project the composed center; the corners are constructed in screen space.
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

        // Keep the animation's anchor below the quad's geometric center.
        workspace->screenExtent = (radiusScale * WATER_TILE_UV_SPAN_TEXELS) / workspace->depth;
        _waterTileSetScreenCorners(quad, workspace);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(workspace->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
