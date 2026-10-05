/* Part of the glow drawing library; see glow_draw.h. */

/// Initializes an allocated cap quad with one lit centre and a black rim.
///
/// Colour stores use the low byte. Coordinates, linkage and blend mode are
/// supplied by the caller; this operation neither allocates nor links a packet.
static inline void _glowInitBeamQuad(POLY_G4* prim, u8 red, u8 green, u8 blue)
{
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, red, green, blue);
    setRGB3(prim, 0, 0, 0);
}

/// Draws a flickering tinted beam between two world points at a fixed screen angle.
///
/// `worldPoints` contains two consecutive points, borrowed during the call.
/// The signed low halfword of `radiusScale` gives each pixel radius as
/// `radiusScale * 64 / depth`, where depth is camera Z / 4. The second point
/// must have depth at least 17; the first depth is clamped to 16.
/// `startAngle` uses its signed low halfword, in 4096 units per turn, zero down.
/// `packedColor` supplies a signed red factor in bits 8..15 and one-bit
/// green/blue factors in bits 4 and 0. They multiply an intensity of 32 or 40
/// on alternating frames; the resulting colour bytes wrap. Queues two cap
/// fans and joining sides as six additive Gouraud quads plus blend commands.
static void _glowDrawBeam(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor)
{
    GlowPointPairScratch* block;
    POLY_G4*              prim;
    const SVECTOR*        secondPoint;
    s32                   sweepAngle;
    s32                   sampleAngle;
    s32                   nextSweepAngle;
    s32                   farRimAngle;
    s32                   shiftedColor;
    s32                   scaledRadius;
    s32                   firstRadius;
    s32                   secondRadius;
    s32                   baseAngle;
    u8                    intensity;
    u8                    red;
    u8                    green;
    u8                    blue;

    secondPoint = worldPoints + 1;
    block       = SCRATCH_STACK_RESERVE_BLOCK(GlowPointPairScratch);

    // Project into the current view before deriving screen radii and sorting depths.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoints);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(secondPoint);
    gte_rtps();
    gte_stsxy(&block->sx1);
    gte_stszotz(&block->otz1);
    if (block->otz1 >= GLOW_MIN_DEPTH) {
        if (block->otz0 < GLOW_NEAR_DEPTH_CLAMP) {
            block->otz0 = GLOW_NEAR_DEPTH_CLAMP;
        }
        scaledRadius   = (s16)radiusScale * GLOW_RADIUS_SCALE;
        firstRadius    = scaledRadius / block->otz0;
        secondRadius   = scaledRadius / block->otz1;
        shiftedColor   = packedColor << 16;
        intensity      = (((u8)gDisplayState.animFrame & 1) * GLOW_FLICKER_INTENSITY_STEP) | GLOW_FLICKER_BASE_INTENSITY;
        red            = intensity * (shiftedColor >> 24);
        green          = intensity * ((shiftedColor >> 20) & 1);
        baseAngle      = (s16)startAngle;
        blue           = intensity * (packedColor & 1);
        sweepAngle     = 0;
        block->radius0 = firstRadius;
        block->radius1 = secondRadius;
        // Each sweep step draws a first cap wedge, a side, and a second cap wedge.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitBeamQuad(prim, red, green, blue);
            prim->x0       = block->sx0 + ((block->radius0 * rsin(baseAngle + sweepAngle)) >> GLOW_TRIG_SHIFT);
            prim->y0       = block->sy0 + ((block->radius0 * rcos(baseAngle + sweepAngle)) >> GLOW_TRIG_SHIFT);
            sampleAngle    = sweepAngle + GLOW_EIGHTH_TURN;
            prim->x1       = block->sx0 + ((block->radius0 * rsin(baseAngle + sampleAngle)) >> GLOW_TRIG_SHIFT);
            prim->y1       = block->sy0 + ((block->radius0 * rcos(baseAngle + sampleAngle)) >> GLOW_TRIG_SHIFT);
            nextSweepAngle = sweepAngle + GLOW_QUARTER_TURN;
            prim->x2       = block->sx0;
            prim->y2       = block->sy0;
            prim->x3       = block->sx0 + ((block->radius0 * rsin(baseAngle + nextSweepAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3       = block->sy0 + ((block->radius0 * rcos(baseAngle + nextSweepAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, red, green, blue);
            setRGB3(prim, red, green, blue);
            prim->x0 = block->sx0 + ((block->radius0 * rsin(baseAngle + (sweepAngle * 2))) >> GLOW_TRIG_SHIFT);
            prim->y0 = block->sy0 + ((block->radius0 * rcos(baseAngle + (sweepAngle * 2))) >> GLOW_TRIG_SHIFT);
            prim->x1 = block->sx1 + ((block->radius1 * rsin(baseAngle + (sweepAngle * 2))) >> GLOW_TRIG_SHIFT);
            prim->y1 = block->sy1 + ((block->radius1 * rcos(baseAngle + (sweepAngle * 2))) >> GLOW_TRIG_SHIFT);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            farRimAngle    = sweepAngle - GLOW_FULL_TURN;
            prim           = gGpuPrimCursor;
            sampleAngle    = sweepAngle - GLOW_FULL_TURN;
            gGpuPrimCursor = prim + 1;
            _glowInitBeamQuad(prim, red, green, blue);
            prim->x0    = block->sx1 + ((block->radius1 * rsin(baseAngle - farRimAngle)) >> GLOW_TRIG_SHIFT);
            prim->y0    = block->sy1 + ((block->radius1 * rcos(baseAngle - sampleAngle)) >> GLOW_TRIG_SHIFT);
            sampleAngle = sweepAngle - (GLOW_FULL_TURN - GLOW_EIGHTH_TURN);
            prim->x1    = block->sx1 + ((block->radius1 * rsin(baseAngle - sampleAngle)) >> GLOW_TRIG_SHIFT);
            prim->y1    = block->sy1 + ((block->radius1 * rcos(baseAngle - sampleAngle)) >> GLOW_TRIG_SHIFT);
            sampleAngle = sweepAngle - (GLOW_FULL_TURN - GLOW_QUARTER_TURN);
            prim->x2    = block->sx1;
            prim->y2    = block->sy1;
            sampleAngle = baseAngle - sampleAngle;
            prim->x3    = block->sx1 + ((block->radius1 * rsin(sampleAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3    = block->sy1 + ((block->radius1 * rcos(sampleAngle)) >> GLOW_TRIG_SHIFT);
            sweepAngle  = nextSweepAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz1);
        } while (sweepAngle < GLOW_HALF_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowPointPairScratch);
}
