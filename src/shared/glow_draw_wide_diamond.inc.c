/* Part of the glow drawing library; see glow_draw.h. */

/// Allocates a centre-lit Gouraud quad in the current frame packet arena.
///
/// Requires one packet's space; colours narrow to bytes. The caller supplies
/// coordinates, ordering-table linkage and the blend command.
static inline POLY_G4* _glowAllocateWideDiamondHalf(s32 cyanIntensity)
{
    POLY_G4* prim;

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, 0, cyanIntensity, cyanIntensity);
    setRGB3(prim, 0, 0, 0);
    return prim;
}

/// Draws a wide pulsing additive cyan diamond with diagonal rays at a world point.
///
/// Borrows `worldPoint` for view projection. A negative GTE flag rejects the
/// draw; camera Z / 4 depth must be nonzero. The signed low halfword of
/// `radiusScale` gives a pixel half-extent of `radiusScale * 48 / depth`.
/// `pulseRate` uses its signed low halfword in 4096 units per turn per frame;
/// cyan intensity is `rsin(animFrame * pulseRate) / 34 + 120`.
///
/// Two quad halves and two three-vertex diagonal rays meet at the lit centre
/// and fade to black. Queues four packets and additive blend commands in the
/// current frame packet arena.
static void _glowDrawWideDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    enum {
        GLOW_WIDE_DIAMOND_PULSE_DIVISOR = 34,
        GLOW_WIDE_DIAMOND_CYAN_BASE     = 120,
        GLOW_WIDE_DIAMOND_RADIUS_SCALE  = 48,
    };

    GlowCentreScratch* previousTop;
    GlowCentreScratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                pulseSine;
    s32                cyanIntensity;
    s32                screenRadius;
    s32                partIndex;
    s32                xRadiusMultiple;
    s32                yRadiusMultiple;
    s32                verticalSide;
    u16                screenX;
    u16                screenY;

    previousTop = SCRATCH_STACK_CURSOR(GlowCentreScratch);
    block       = (SCRATCH_STACK_CURSOR(GlowCentreScratch) = previousTop - 1);

    // Project once; all four packets share the resulting centre and depth.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&previousTop[-1].sx);
    gte_stflg(&previousTop[-1].flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulseSine     = rsin(gDisplayState.animFrame * (s16)pulseRate);
        screenRadius  = ((s16)radiusScale * GLOW_WIDE_DIAMOND_RADIUS_SCALE) / previousTop[-1].otz;
        partIndex     = 0;
        cyanIntensity = pulseSine / GLOW_WIDE_DIAMOND_PULSE_DIVISOR + GLOW_WIDE_DIAMOND_CYAN_BASE;
        block->radius = screenRadius;
        // Fill the diamond with two centre-lit halves.
        do {
            prim         = _glowAllocateWideDiamondHalf(cyanIntensity);
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

        // Overlay two diagonal rays through the same lit centre.
        partIndex = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, cyanIntensity, cyanIntensity);
            setRGB2(line, 0, 0, 0);
            xRadiusMultiple = partIndex * 3 - 1;
            yRadiusMultiple = partIndex + 1;
            line->x0        = block->sx + (block->radius * xRadiusMultiple);
            line->y0        = block->sy - (block->radius * yRadiusMultiple);
            line->x1        = block->sx;
            line->y1        = block->sy;
            line->x2        = block->sx - (block->radius * xRadiusMultiple);
            line->y2        = block->sy + (block->radius * yRadiusMultiple);
            addPrim((&gGpuCurrentOt[((u32)block->otz << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)]),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            partIndex = yRadiusMultiple;
        } while (partIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreScratch);
}
