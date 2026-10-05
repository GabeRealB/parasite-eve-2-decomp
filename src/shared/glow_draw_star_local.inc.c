/* Part of the glow drawing library; see glow_draw.h. */

/// Allocates a centre-lit Gouraud quad in the current frame packet arena.
///
/// Requires one packet's space; colours narrow to bytes. The caller supplies
/// coordinates, ordering-table linkage and the blend command.
static inline POLY_G4* _glowAllocateLocalStarHalf(s32 cyanIntensity)
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

/// Draws a pulsing additive cyan diamond with diagonal rays at a local point.
///
/// Composes `coord`, then rotates and translates the borrowed `localPoint`
/// into world components narrowed to signed 16 bits. Projects through
/// `GsWSMATRIX` and requires camera Z / 4 depth of at least 17; projection
/// flags are not tested. The signed low halfword of `radiusScale` gives a
/// pixel half-extent of `radiusScale * 32 / depth`.
///
/// `pulseRate` uses its signed low halfword in 4096 units per turn per frame;
/// cyan intensity is `rsin(animFrame * pulseRate) / 34 + 120`. Two quad halves
/// and two three-vertex diagonal rays meet at the lit centre and fade to black.
/// Queues four packets and additive blend commands in the current frame arena.
static void _glowDrawStarLocal(GfxCoord* coord, const SVECTOR* localPoint, s32 pulseRate, s32 radiusScale)
{
    enum {
        GLOW_LOCAL_STAR_PULSE_DIVISOR = 34,
        GLOW_LOCAL_STAR_CYAN_BASE     = 120,
        GLOW_LOCAL_STAR_MIN_DEPTH     = 17,
        GLOW_LOCAL_STAR_RADIUS_SHIFT  = 5,
    };

    RoomGlowSpriteScratch* block;
    POLY_G4*               prim;
    LINE_G3*               line;
    s32                    partIndex;
    s32                    cyanIntensity;
    s32                    pulseSine;
    s32                    verticalSide;
    s32                    xRadiusMultiple;
    s32                    yRadiusMultiple;

    // Composition updates the coordinate; the borrowed local point remains unchanged.
    actorRenderComposeCoord(coord);
    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(localPoint);
    gte_rtv0();
    gte_stsv(&block->worldPos);
    block->worldPos.vx += coord->workm.t[0];
    block->worldPos.vy += coord->workm.t[1];
    block->worldPos.vz += coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= GLOW_LOCAL_STAR_MIN_DEPTH) {
        pulseSine         = rsin(gDisplayState.animFrame * (s16)pulseRate);
        partIndex         = 0;
        block->halfExtent = ((s16)radiusScale << GLOW_LOCAL_STAR_RADIUS_SHIFT) / block->otz;
        cyanIntensity     = pulseSine / GLOW_LOCAL_STAR_PULSE_DIVISOR + GLOW_LOCAL_STAR_CYAN_BASE;
        // Fill the diamond with two centre-lit halves.
        do {
            prim     = _glowAllocateLocalStarHalf(cyanIntensity);
            prim->x0 = block->screenPos.vx - block->halfExtent;
            prim->x1 = prim->x2 = block->screenPos.vx;
            prim->x3            = block->screenPos.vx + block->halfExtent;
            prim->y0 = prim->y2 = prim->y3 = block->screenPos.vy;
            verticalSide                   = partIndex << 1;
            prim->y1                       = (block->screenPos.vy - block->halfExtent) + block->halfExtent * verticalSide;
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
            line->x0        = block->screenPos.vx + (block->halfExtent * xRadiusMultiple);
            line->y0        = block->screenPos.vy - (block->halfExtent * yRadiusMultiple);
            line->x1        = block->screenPos.vx;
            line->y1        = block->screenPos.vy;
            line->x2        = block->screenPos.vx - (block->halfExtent * xRadiusMultiple);
            line->y2        = block->screenPos.vy + (block->halfExtent * yRadiusMultiple);
            addPrim((&gGpuCurrentOt[((u32)block->otz << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)]),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            partIndex = yRadiusMultiple;
        } while (partIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}
