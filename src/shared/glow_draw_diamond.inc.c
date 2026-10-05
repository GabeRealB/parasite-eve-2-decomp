/* Part of the glow drawing library; see glow_draw.h. */

/// Prepares one half of a world-point diamond with a cyan centre and a black rim.
///
/// Borrows one writable `POLY_G4`, setting its opaque Gouraud command and
/// packet length. Vertex 2 has zero red and the low byte of `cyanIntensity`
/// in green and blue; vertices 0, 1 and 3 are black. The caller supplies the
/// three outer corners, centre coordinates, DMA linkage and semitransparency.
static inline void _glowInitDiamondHalf(POLY_G4* diamondHalf, s32 cyanIntensity)
{
    setPolyG4(diamondHalf);
    setRGB0(diamondHalf, 0, 0, 0);
    setRGB1(diamondHalf, 0, 0, 0);
    setRGB2(diamondHalf, 0, cyanIntensity, cyanIntensity);
    setRGB3(diamondHalf, 0, 0, 0);
}

/// Draws a pulsing cyan diamond with two diagonals around a world point.
///
/// The signed low halfword of `radiusScale` gives a pixel half-extent of
/// `radiusScale * 32 / depth`, where depth is camera Z / 4. The signed low
/// halfword of `pulseRate` is in 4096 angle units per animation frame;
/// green and blue pulse as `rsin(animFrame * pulseRate) / 34 + 120`.
/// Two Gouraud quads fill the diamond, then two three-vertex lines cross its
/// centre; the second line extends twice as far. Rejects negative GTE flags
/// and requires nonzero depth. Borrows `worldPoint` during the call and queues
/// four additive packets plus blend commands in the current frame.
static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    GlowCentreScratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                pulseSine;
    s32                intensity;
    s32                screenRadius;
    s32                partIndex;
    s32                xRadiusMultiple;
    s32                yRadiusMultiple;
    s32                verticalSide;
    u16                screenX;
    u16                screenY;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreScratch);

    // Project into the current view before deriving screen radii and sorting depths.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulseSine     = rsin(gDisplayState.animFrame * (s16)pulseRate);
        screenRadius  = ((s16)radiusScale * GLOW_DIAMOND_RADIUS_SCALE) / block->otz;
        partIndex     = 0;
        intensity     = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        block->radius = screenRadius;
        // Two Gouraud halves fill the diamond.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitDiamondHalf(prim, intensity);
            prim->x0     = block->sx - (u16)block->radius;
            screenX      = block->sx;
            prim->x2     = screenX;
            prim->x1     = screenX;
            prim->x3     = block->sx + (u16)block->radius;
            screenY      = block->sy;
            prim->y3     = screenY;
            prim->y2     = screenY;
            prim->y0     = screenY;
            verticalSide = partIndex * 2;
            prim->y1     = (block->sy - (u16)block->radius) + (block->radius * verticalSide);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
            partIndex++;
        } while (partIndex < 2);

        // The second diagonal extends twice as far as the diamond.
        partIndex = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, intensity, intensity);
            setRGB2(line, 0, 0, 0);
            xRadiusMultiple = partIndex * 3 - 1;
            yRadiusMultiple = partIndex + 1;
            line->x0        = block->sx + (block->radius * xRadiusMultiple);
            line->y0        = block->sy - (block->radius * yRadiusMultiple);
            line->x1        = block->sx;
            line->y1        = block->sy;
            line->x2        = block->sx - (block->radius * xRadiusMultiple);
            line->y2        = block->sy + (block->radius * yRadiusMultiple);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 4) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)) * sizeof(*gGpuCurrentOt)),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            partIndex = yRadiusMultiple;
        } while (partIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreScratch);
}
