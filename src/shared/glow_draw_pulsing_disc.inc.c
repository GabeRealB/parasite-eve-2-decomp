/* Part of the glow drawing library; see glow_draw.h. */

/// Prepares a pulsing disc wedge or blade with a cyan centre and a black rim.
///
/// Borrows one writable `POLY_G4`, setting its opaque Gouraud command and
/// packet length. Vertex 2 has zero red and the low byte of `cyanIntensity`
/// in green and blue; vertices 0, 1 and 3 are black. The caller supplies
/// coordinates, DMA linkage and semitransparency for the inner disc and blades.
static inline void _glowInitPulsingDiscWedge(POLY_G4* wedge, s32 cyanIntensity)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, 0, cyanIntensity, cyanIntensity);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws layered pulsing cyan discs and four glow blades around a world point.
///
/// The signed low halfword of `radiusScale` gives outer and inner pixel radii
/// of `radiusScale * 64 / depth` and `radiusScale * 8 / depth`, where depth is
/// camera Z / 4. Eight half-bright outer wedges receive full-bright copies at
/// half radius. Four half-bright blades overlay them, with alternating tips
/// at the outer radius and twice it, with shoulders at half the inner radius
/// on shorter blades.
/// The signed low halfword of `pulseRate` is in 4096 angle units per animation
/// frame; green and blue pulse as `rsin(animFrame * pulseRate) / 34 + 120`.
/// Rejects negative GTE flags and requires nonzero depth. Borrows `worldPoint`
/// for the call and queues twenty additive quads plus blend commands.
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    GlowCentreRadiiScratch* block;
    POLY_G4*                prim;
    s32                     pulseSine;
    s32                     intensity;
    s32                     halfIntensity;
    s32                     signedRadiusScale;
    s32                     angle;
    s32                     halfStepAngle;
    s32                     nextAngle;
    s32                     armAngle;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiiScratch);

    // Project into the current view before deriving screen radii and sorting depths.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulseSine          = rsin(gDisplayState.animFrame * (s16)pulseRate);
        angle              = 0;
        signedRadiusScale  = (s16)radiusScale;
        block->outerRadius = (signedRadiusScale * GLOW_RADIUS_SCALE) / block->otz;
        intensity          = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        block->innerRadius = (signedRadiusScale * GLOW_INNER_RADIUS_SCALE) / block->otz;
        // Layer eight outer wedges with full-bright copies at half radius.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            halfIntensity = (s16)intensity >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, halfIntensity, halfIntensity);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_SIXTEENTH_TURN;
            prim->y0      = block->sy + ((block->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_EIGHTH_TURN;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->outerRadius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->outerRadius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitPulsingDiscWedge(prim, intensity);
            prim->x0 = block->sx + ((block->outerRadius * rsin(angle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y0 = block->sy + ((block->outerRadius * rcos(angle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->x1 = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y1 = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->outerRadius * rsin(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y3 = block->sy + ((block->outerRadius * rcos(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            angle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);

        // Overlay four blades with alternating normal and doubled tips.
        intensity = halfIntensity;
        angle     = GLOW_EIGHTH_TURN;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitPulsingDiscWedge(prim, intensity);
            armAngle = angle - GLOW_QUARTER_TURN;
            prim->x0 = block->sx + ((block->innerRadius * rsin(armAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y0 = block->sy + ((block->innerRadius * rcos(armAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->x1 = block->sx + ((block->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            prim->y1 = block->sy + ((block->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            armAngle = angle + GLOW_QUARTER_TURN;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(armAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y3 = block->sy + ((block->innerRadius * rcos(armAngle)) >> (GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitPulsingDiscWedge(prim, intensity);
            prim->x0 = block->sx + ((block->innerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            prim->y0 = block->sy + ((block->innerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            prim->x1 = block->sx + ((block->outerRadius * rsin(armAngle)) >> (GLOW_TRIG_SHIFT - 1));
            prim->y1 = block->sy + ((block->outerRadius * rcos(armAngle)) >> (GLOW_TRIG_SHIFT - 1));
            armAngle = angle + GLOW_HALF_TURN;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(armAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3 = block->sy + ((block->innerRadius * rcos(armAngle)) >> GLOW_TRIG_SHIFT);
            angle    = armAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiiScratch);
}
