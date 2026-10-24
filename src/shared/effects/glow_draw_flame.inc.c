/* Part of the glow drawing library; see glow_draw.h. */

/// Projects one cone segment's four world corners with the current GTE matrices.
///
/// `segmentIndex` must be 0..`EFFECT_BAND_SEGMENT_COUNT - 1`; the next index
/// wraps to zero. Stores the top pair then the bottom pair in `sxy0`..`sxy3`.
/// `projectionFlags` describes only the final three-corner RTPT, not the first
/// RTPS. Leaves the last bottom corner's depth in SZ3 for the caller to capture.
/// Borrows the live scratch block for this call and reserves no storage.
static inline void _glowProjectFlameConeSegment(EffectBandScratch* block, s32 segmentIndex)
{
    s32 nextIndex;

    gte_ldv0(&block->topRing[segmentIndex]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    nextIndex = (segmentIndex + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
    gte_ldv3(&block->topRing[nextIndex], &block->bottomRing[segmentIndex], &block->bottomRing[nextIndex]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->projectionFlags);
}

void glowDrawFlameCone(const GfxCoord* coord, s16 innerRadius, s16 intensity)
{
    enum {
        GLOW_FLAME_CONE_TRIG_SHIFT      = 12,
        GLOW_FLAME_CONE_RING_STEP_SHIFT = 8,
        GLOW_FLAME_CONE_SPAN            = 0x100,
    };

    EffectBandScratch* block;
    SVECTOR*           outerVertex;
    POLY_G4*           prim;
    s32                segmentIndex;
    s32                angle;
    s16                ringRadius;
    s16                outerRadius;
    u32                shiftedIntensity;
    u8                 red;
    u8                 green;
    u8                 blue;

    // Widen before halving so green and blue use the unsigned low halfword.
    shiftedIntensity = (u32)intensity << 16;
    red              = intensity;
    green            = shiftedIntensity >> 17;
    blue             = shiftedIntensity >> 18;
    outerRadius      = innerRadius + GLOW_FLAME_CONE_SPAN;
    block            = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    ringRadius = innerRadius;
    // Build the lit raised ring and the wider dark rim in world coordinates.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        angle                           = segmentIndex << GLOW_FLAME_CONE_RING_STEP_SHIFT;
        block->topRing[segmentIndex].vx = (rsin(angle) * ringRadius) >> GLOW_FLAME_CONE_TRIG_SHIFT;
        block->topRing[segmentIndex].vy = (rcos(angle) * ringRadius) >> GLOW_FLAME_CONE_TRIG_SHIFT;
        block->topRing[segmentIndex].vz = GLOW_FLAME_CONE_SPAN;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&block->topRing[segmentIndex]);
        block->topRing[segmentIndex].vx    = (u16)block->topRing[segmentIndex].vx + (u16)coord->workm.t[0];
        block->topRing[segmentIndex].vy    = (u16)block->topRing[segmentIndex].vy + (u16)coord->workm.t[1];
        block->topRing[segmentIndex].vz    = (u16)block->topRing[segmentIndex].vz + (u16)coord->workm.t[2];
        block->bottomRing[segmentIndex].vx = (rsin(angle) * outerRadius) >> GLOW_FLAME_CONE_TRIG_SHIFT;
        // Address the outer vertex through a byte view of the complete scratch block.
        outerVertex     = (SVECTOR*)((u8*)block + segmentIndex * sizeof(SVECTOR) + sizeof(block->topRing));
        outerVertex->vy = (rcos(angle) * outerRadius) >> GLOW_FLAME_CONE_TRIG_SHIFT;
        outerVertex->vz = 0;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[segmentIndex]);
        block->bottomRing[segmentIndex].vx = (u16)block->bottomRing[segmentIndex].vx + (u16)coord->workm.t[0];
        outerVertex->vy                    = (u16)outerVertex->vy + (u16)coord->workm.t[1];
        outerVertex->vz                    = (u16)outerVertex->vz + (u16)coord->workm.t[2];
    }
    // Project and queue each segment separately; rejected segments emit no packets.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        _glowProjectFlameConeSegment(block, segmentIndex);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, red, green, blue);
            setRGB1(prim, red, green, blue);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

/// Allocates a flame-coloured, centre-lit Gouraud wedge in the frame packet arena.
///
/// Requires space for one `POLY_G4`. Vertex 2 uses `(intensity, intensity >> 1,
/// intensity >> 2)` with signed shifts and byte narrowing; vertices 0/1/3 are
/// black. Normal intensity is 0..255. The caller supplies coordinates, the
/// ordering-table link and blend command. The packet lives until GPU completion.
static inline POLY_G4* _glowAllocateFlameDiscWedge(s16 intensity)
{
    POLY_G4* prim;

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, intensity, intensity >> 1, intensity >> 2);
    setRGB3(prim, 0, 0, 0);
    return prim;
}

void glowDrawFlameDisc(const GfxCoord* coord, s16 radiusScale, s16 intensity)
{
    enum {
        GLOW_FLAME_DISC_TRIG_SHIFT   = 12,
        GLOW_FLAME_DISC_FULL_TURN    = 0x1000,
        GLOW_FLAME_DISC_WEDGE_ANGLE  = 0x200,
        GLOW_FLAME_DISC_RADIUS_SCALE = 64,
    };

    EffectCentreScratch* block;
    POLY_G4*             prim;
    s32                  angle;

    // Only the coordinate origin is used; the disc faces the screen.
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        block->screenExtent = (radiusScale * GLOW_FLAME_DISC_RADIUS_SCALE) / block->depth;
        for (angle = 0; angle < GLOW_FLAME_DISC_FULL_TURN; angle += GLOW_FLAME_DISC_WEDGE_ANGLE) {
            prim     = _glowAllocateFlameDiscWedge(intensity);
            prim->x0 = block->screenX + ((block->screenExtent * rsin(angle)) >> GLOW_FLAME_DISC_TRIG_SHIFT);
            prim->y0 = block->screenY + ((block->screenExtent * rcos(angle)) >> GLOW_FLAME_DISC_TRIG_SHIFT);
            prim->x1 = block->screenX + ((block->screenExtent * rsin(angle + GLOW_FLAME_DISC_WEDGE_ANGLE / 2)) >> GLOW_FLAME_DISC_TRIG_SHIFT);
            prim->y1 = block->screenY + ((block->screenExtent * rcos(angle + GLOW_FLAME_DISC_WEDGE_ANGLE / 2)) >> GLOW_FLAME_DISC_TRIG_SHIFT);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->screenExtent * rsin(angle + GLOW_FLAME_DISC_WEDGE_ANGLE)) >> GLOW_FLAME_DISC_TRIG_SHIFT);
            prim->y3 = block->screenY + ((block->screenExtent * rcos(angle + GLOW_FLAME_DISC_WEDGE_ANGLE)) >> GLOW_FLAME_DISC_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
