/* Part of the pyro flame library; see pyro_flame.h. */

/// Stores the pixel offset from the flame's center to the corner at `cornerAngle` (zero up, a quarter turn right),
/// its size scaled by depth.
static inline void _pyroFlameComputeCornerOffset(EffectShapeScratch* scratch, s16 sizeFactor, s32 cornerAngle)
{
    s32    halfDiagonalPixels;
    q19_12 trigSample;

    trigSample               = rsin(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * PYRO_FLAME_UV_SPAN) / scratch->depth;
    scratch->extent.corner.x = (halfDiagonalPixels * trigSample) >> PYRO_FLAME_TRIG_FRACTION_BITS;
    trigSample               = rcos(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * PYRO_FLAME_UV_SPAN) / scratch->depth;
    scratch->extent.corner.y = (halfDiagonalPixels * trigSample) >> PYRO_FLAME_TRIG_FRACTION_BITS;
}

/// Draws animation frame `frame` (0..7) of the additive flame billboard at `coord`, `sizeFactor` big and turned
/// by `angle`.
static void _pyroFlameDrawSprite(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 angle)
{
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    s32                 leftU;
    s32                 rightU;
    s32                 perpendicularAngle;

    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    // Project the composed center; the flame rotates in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        // Bias the sizing and ordering depth to keep the perspective divisor nonzero.
        scratch->depth += PYRO_FLAME_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
        quad->clut  = getClut(32, 267);
        leftU       = frame * PYRO_FLAME_CELL_WIDTH;
        rightU      = leftU + PYRO_FLAME_UV_SPAN;
        setUV4(quad, leftU, PYRO_FLAME_TOP_V, rightU, PYRO_FLAME_TOP_V, leftU, PYRO_FLAME_BOTTOM_V, rightU, PYRO_FLAME_BOTTOM_V);
        // Two perpendicular half-diagonals supply the opposite corner pairs.
        _pyroFlameComputeCornerOffset(scratch, sizeFactor, angle);
        quad->x0           = scratch->screenX + scratch->extent.corner.x;
        quad->x3           = scratch->screenX - scratch->extent.corner.x;
        quad->y0           = scratch->screenY - scratch->extent.corner.y;
        quad->y3           = scratch->screenY + scratch->extent.corner.y;
        perpendicularAngle = angle + PYRO_FLAME_QUARTER_TURN;
        _pyroFlameComputeCornerOffset(scratch, sizeFactor, perpendicularAngle);
        quad->x1 = scratch->screenX + scratch->extent.corner.x;
        quad->x2 = scratch->screenX - scratch->extent.corner.x;
        quad->y1 = scratch->screenY - scratch->extent.corner.y;
        quad->y2 = scratch->screenY + scratch->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
