/* Part of the glow drawing library; see glow_draw.h. */

/// Prepares a capsule's cap wedge with an RGB centre and a black rim.
///
/// Borrows one writable `POLY_G4`, setting its opaque Gouraud command and
/// packet length. Vertex 2 receives the supplied colour bytes; vertices 0, 1
/// and 3 are black. The caller supplies coordinates, DMA linkage and
/// semitransparency for this end-cap packet in every capsule variant.
static inline void _glowInitCapsuleWedge(POLY_G4* capWedge, u8 red, u8 green, u8 blue)
{
    setPolyG4(capWedge);
    setRGB0(capWedge, 0, 0, 0);
    setRGB1(capWedge, 0, 0, 0);
    setRGB2(capWedge, red, green, blue);
    setRGB3(capWedge, 0, 0, 0);
}

/// Draws a tinted capsule aligned with the projected line between two world points.
///
/// `worldPoints` contains two consecutive points, borrowed during the call.
/// Negative GTE flags at either point reject the capsule. The signed low
/// halfword of `radiusScale` gives each pixel radius as `radiusScale * 64 / depth`,
/// where depth is camera Z / 4 and must be nonzero. `packedColor` bits 8..11,
/// 4..7 and 0..3 supply RGB nibbles scaled by 16; odd frames insert bit 3.
/// The shifted-flicker variant instead adds `1 << shift` to each byte, with
/// shift in bits 12..15 restricted to 0..7. Colour bytes wrap.
/// The pull variant subtracts `GLOW_DRAW_CAPSULE_PULL` from depths above 0x50
/// before sizing and sorting. Queues six additive Gouraud quads plus blend
/// commands; the joining quads sort at the mean of the two end depths.
static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor)
{
    enum { GLOW_CAPSULE_PULL_THRESHOLD = 0x50 };

    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    const DisplayState*      display;
    const SVECTOR*           secondPoint;
    s32                      sweepAngle;
    s32                      sampleAngle;
    s32                      farRimAngle;
    s32                      nextSweepAngle;
    s32                      sweepLimit;
    s32                      baseAngle;
    s32                      shiftedColor;
    s32                      flicker;
    s32                      redNibble;
    s32                      greenNibble;
    s32                      scaledRadius;
    s32                      sideAngle;
#ifdef GLOW_DRAW_CAPSULE_PULL
    s32 originalDepth;
#endif
    u8 red;
    u8 green;
    u8 blue;

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
#ifdef GLOW_DRAW_CAPSULE_PULL
        originalDepth = block->otz0;
        if (originalDepth > GLOW_CAPSULE_PULL_THRESHOLD) {
            block->otz0 = originalDepth - GLOW_DRAW_CAPSULE_PULL;
        }
#endif
        gte_ldv0(secondPoint);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
#ifdef GLOW_DRAW_CAPSULE_PULL
            originalDepth = block->otz1;
            if (originalDepth > GLOW_CAPSULE_PULL_THRESHOLD) {
                block->otz1 = originalDepth - GLOW_DRAW_CAPSULE_PULL;
            }
#endif
            scaledRadius   = (s16)radiusScale * GLOW_RADIUS_SCALE;
            block->radius0 = scaledRadius / block->otz0;
            block->radius1 = scaledRadius / block->otz1;
            sweepAngle     = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            display        = &gDisplayState;
#ifdef GLOW_DRAW_CAPSULE_SHIFTED_FLICKER
            // Add the encoded odd-frame flicker to each colour channel.
            flicker      = display->animFrame;
            shiftedColor = packedColor << 16;
            flicker      = flicker & 1;
            sweepAngle   = (s16)sweepAngle;
            flicker      = flicker << (shiftedColor >> 28);
            redNibble    = (shiftedColor >> 20) & 0xF0;
            greenNibble  = (shiftedColor >> 16) & 0xF0;
            red          = flicker + redNibble;
            green        = flicker + greenNibble;
            blue         = flicker + ((packedColor & 0xF) << 4);
#else
            sweepAngle   = (s16)sweepAngle;
            flicker      = ((u8)display->animFrame & 1) * GLOW_FLICKER_INTENSITY_STEP;
            shiftedColor = packedColor << 16;
            redNibble    = (shiftedColor >> 20) & 0xF0;
            greenNibble  = (shiftedColor >> 16) & 0xF0;
            red          = flicker | redNibble;
            green        = flicker | greenNibble;
            blue         = flicker | ((packedColor & 0xF) << 4);
#endif
            if (sweepAngle < sweepAngle + GLOW_HALF_TURN) {
                baseAngle  = sweepAngle;
                sweepLimit = sweepAngle + GLOW_HALF_TURN;
                // Each sweep step draws a first cap wedge, a side, and a second cap wedge.
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    _glowInitCapsuleWedge(prim, red, green, blue);
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

                    prim           = gGpuPrimCursor;
                    sideAngle      = baseAngle + (sweepAngle - baseAngle) * 2;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, green, blue);
                    setRGB3(prim, red, green, blue);
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
                    farRimAngle    = sweepAngle + GLOW_HALF_TURN;
                    prim           = gGpuPrimCursor;
                    sampleAngle    = farRimAngle;
                    gGpuPrimCursor = prim + 1;
                    _glowInitCapsuleWedge(prim, red, green, blue);
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

#undef GLOW_DRAW_CAPSULE_PULL
#undef GLOW_DRAW_CAPSULE_SHIFTED_FLICKER
