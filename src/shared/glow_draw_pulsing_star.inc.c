/* Part of the glow drawing library; see glow_draw.h. */

/// Reserves a Gouraud quad with a coloured centre and a black rim.
static inline POLY_G4* _glowAllocatePulsingStarHalf(s32 redIntensity)
{
    POLY_G4* prim;

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, redIntensity, 0, 0);
    setRGB3(prim, 0, 0, 0);
    return prim;
}

void glowDrawPulsingStar(const SVECTOR* worldPoint, s16 pulseRate, s32 radiusScale)
{
    enum {
        GLOW_PULSING_STAR_RADIUS_SCALE  = 32,
        GLOW_PULSING_STAR_PULSE_DIVISOR = 34,
        GLOW_PULSING_STAR_BASE_RED      = 0x78,
    };

    GlowCentreScratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                pulseSine;
    s32                redIntensity;
    s32                screenRadius;
    s32                partIndex;
    s32                xRadiusMultiple;
    s32                yRadiusMultiple;
    s32                verticalSide;
    u16                screenX;
    u16                screenY;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulseSine     = rsin(gDisplayState.animFrame * pulseRate);
        screenRadius  = ((s16)radiusScale * GLOW_PULSING_STAR_RADIUS_SCALE) / block->otz;
        partIndex     = 0;
        redIntensity  = pulseSine / GLOW_PULSING_STAR_PULSE_DIVISOR + GLOW_PULSING_STAR_BASE_RED;
        block->radius = screenRadius;
        // Two triangular halves fill the diamond.
        do {
            prim         = _glowAllocatePulsingStarHalf(redIntensity);
            prim->x0     = block->sx - block->radius;
            screenX      = block->sx;
            prim->x2     = screenX;
            prim->x1     = screenX;
            prim->x3     = block->sx + block->radius;
            screenY      = block->sy;
            prim->y3     = screenY;
            prim->y2     = screenY;
            prim->y0     = screenY;
            verticalSide = partIndex * 2;
            prim->y1     = (block->sy - block->radius) + (block->radius * verticalSide);
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
            setRGB1(line, redIntensity, 0, 0);
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
