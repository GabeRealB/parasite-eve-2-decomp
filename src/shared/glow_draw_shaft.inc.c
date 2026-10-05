/* Part of the glow drawing library; see glow_draw.h. */

/// Initializes an allocated cap quad with one lit centre and a black rim.
///
/// Colour stores use the low byte. Coordinates, linkage and blend mode are
/// supplied by the caller; this operation neither allocates nor links a packet.
static inline void _glowInitShaftQuad(POLY_G4* prim, s32 intensity)
{
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, intensity, intensity, intensity);
    setRGB3(prim, 0, 0, 0);
}

/// Draws a grey light shaft aligned with the projected line between two world points.
///
/// `worldPoints` contains two consecutive points, borrowed during the call.
/// Negative GTE flags at either end reject the shaft. The signed low halfword
/// of `radiusScale` gives each pixel radius as `radiusScale * 64 / depth`,
/// where depth is camera Z / 4 and must be nonzero. Centre intensity alternates
/// between 32 and 48; the rims are black. Queues six additive Gouraud quads
/// plus blend commands, with joining quads sorted at the mean end depth.
static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale)
{
    const SVECTOR*           secondPoint;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    const DisplayState*      display;
    s32                      axisAngle;
    s32                      sweepAngle;
    s32                      endAngle;
    s32                      sweepLimit;
    s32                      baseAngle;
    s32                      sampleAngle;
    s32                      nextSweepAngle;
    s32                      farRimAngle;
    s32                      sideAngle;
    s32                      scaledRadius;
    s32                      intensity;

    secondPoint = worldPoints + 1;
    block       = SCRATCH_STACK_RESERVE_BLOCK(OverlayPointPairScratch);

    // Project into the current view before deriving screen radii and sorting depths.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoints);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(secondPoint);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaledRadius   = (s16)radiusScale * GLOW_RADIUS_SCALE;
            block->radius0 = scaledRadius / block->otz0;
            block->radius1 = scaledRadius / block->otz1;
            axisAngle      = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            display        = &gDisplayState;
            sweepAngle     = (s16)axisAngle;
            intensity      = (((u8)display->animFrame & 1) * (1 << GLOW_BRIGHT_FLICKER_SHIFT)) | GLOW_FLICKER_BASE_INTENSITY;
            endAngle       = sweepAngle + GLOW_HALF_TURN;
            if (sweepAngle < endAngle) {
                baseAngle  = sweepAngle;
                sweepLimit = endAngle;
                // Each sweep step draws a first cap wedge, a side, and a second cap wedge.
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    _glowInitShaftQuad(prim, intensity);
                    prim->x0       = block->sx0 + ((block->radius0 * rsin(sweepAngle)) >> GLOW_TRIG_SHIFT);
                    sampleAngle    = sweepAngle + GLOW_EIGHTH_TURN;
                    prim->y0       = block->sy0 + ((block->radius0 * rcos(sweepAngle)) >> GLOW_TRIG_SHIFT);
                    prim->x1       = block->sx0 + ((block->radius0 * rsin(sampleAngle)) >> GLOW_TRIG_SHIFT);
                    prim->y1       = block->sy0 + ((block->radius0 * rcos(sampleAngle)) >> GLOW_TRIG_SHIFT);
                    nextSweepAngle = sweepAngle + GLOW_QUARTER_TURN;
                    prim->x2       = block->sx0;
                    prim->y2       = block->sy0;
                    prim->x3       = block->sx0 + ((block->radius0 * rsin(nextSweepAngle)) >> GLOW_TRIG_SHIFT);
                    prim->y3       = block->sy0 + ((block->radius0 * rcos(nextSweepAngle)) >> GLOW_TRIG_SHIFT);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

                    sideAngle      = baseAngle + ((sweepAngle - baseAngle) * 2);
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, intensity, intensity, intensity);
                    setRGB3(prim, intensity, intensity, intensity);
                    prim->x0 = block->sx0 + ((block->radius0 * rsin(sideAngle)) >> GLOW_TRIG_SHIFT);
                    prim->y0 = block->sy0 + ((block->radius0 * rcos(sideAngle)) >> GLOW_TRIG_SHIFT);
                    prim->x1 = block->sx1 + ((block->radius1 * rsin(sideAngle)) >> GLOW_TRIG_SHIFT);
                    prim->y1 = block->sy1 + ((block->radius1 * rcos(sideAngle)) >> GLOW_TRIG_SHIFT);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, (block->otz1 + block->otz0) / 2);

                    prim           = gGpuPrimCursor;
                    farRimAngle    = sweepAngle + GLOW_HALF_TURN;
                    sampleAngle    = farRimAngle;
                    gGpuPrimCursor = prim + 1;
                    _glowInitShaftQuad(prim, intensity);
                    prim->x0    = block->sx1 + ((block->radius1 * rsin(sampleAngle)) >> GLOW_TRIG_SHIFT);
                    prim->y0    = block->sy1 + ((block->radius1 * rcos(sampleAngle)) >> GLOW_TRIG_SHIFT);
                    sampleAngle = sweepAngle + GLOW_HALF_TURN + GLOW_EIGHTH_TURN;
                    prim->x1    = block->sx1 + ((block->radius1 * rsin(sampleAngle)) >> GLOW_TRIG_SHIFT);
                    prim->y1    = block->sy1 + ((block->radius1 * rcos(sampleAngle)) >> GLOW_TRIG_SHIFT);
                    sampleAngle = sweepAngle + GLOW_HALF_TURN + GLOW_QUARTER_TURN;
                    prim->x2    = block->sx1;
                    prim->y2    = block->sy1;
                    prim->x3    = block->sx1 + ((block->radius1 * rsin(sampleAngle)) >> GLOW_TRIG_SHIFT);
                    prim->y3    = block->sy1 + ((block->radius1 * rcos(sampleAngle)) >> GLOW_TRIG_SHIFT);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz1);
                    sweepAngle = nextSweepAngle;
                } while (sweepAngle < sweepLimit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayPointPairScratch);
}
