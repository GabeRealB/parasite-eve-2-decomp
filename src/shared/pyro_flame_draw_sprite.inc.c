/* Part of the pyro flame library; see pyro_flame.h. */

/// Writes one rotated half-diagonal's pixel offsets, with X rightward and Y upward.
///
/// Borrows live scratch storage with positive depth. The signed divide truncates
/// before the Q12 rotation; only `extent.corner` is written. Signed products must
/// fit in 32 bits, and the arithmetic shift rounds negative products down.
static inline void _pyroFlameComputeCornerOffset(EffectShapeScratch* scratch, s16 sizeFactor, s32 cornerAngle)
{
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample               = rsin(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * PYRO_FLAME_UV_SPAN) / scratch->depth;
    scratch->extent.corner.x = (halfDiagonalPixels * trigSample) >> PYRO_FLAME_TRIG_FRACTION_BITS;
    trigSample               = rcos(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * PYRO_FLAME_UV_SPAN) / scratch->depth;
    scratch->extent.corner.y = (halfDiagonalPixels * trigSample) >> PYRO_FLAME_TRIG_FRACTION_BITS;
}

/// Queues one additive, unmodulated frame of the rotating flame billboard.
///
/// `coord` must have a composed view-space translation in `workm`; its low
/// 16 bits are read as signed coordinates and projected through `GsWSMATRIX`.
/// `frame` is in 0..7, selecting a 32-by-32 cell.
/// `sizeFactor * 31 / (SZ3 / 4 + 1)` is the screen half-diagonal
/// in pixels before rotation. `angle` uses 4096 units per turn: zero puts the
/// first corner above the centre, and a quarter turn puts it to the right.
/// Signed rotation products must fit in 32 bits.
///
/// Requires initialized GTE projection settings, a word-aligned scratch-stack
/// cursor with room for one `EffectShapeScratch`, and space for one `POLY_FT4`
/// in the primitive arena. A negative GTE FLAG rejects the sprite. Scratch
/// storage is released on both paths; a queued packet remains in the current
/// frame's primitive arena for GPU consumption. The coordinate is only borrowed.
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
    // Project the composed centre; the flame rotates in screen space.
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
