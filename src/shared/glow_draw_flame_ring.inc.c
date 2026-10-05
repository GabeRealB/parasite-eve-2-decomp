/* Part of the glow drawing library; see glow_draw.h. */

/// Projects one flat ring segment's four world corners with the current GTE matrices.
///
/// `segmentIndex` must be 0..`EFFECT_BAND_SEGMENT_COUNT - 1`; the next index
/// wraps to zero. Stores the inner pair then the outer pair in `sxy0`..`sxy3`.
/// `projectionFlags` describes only the final three-corner RTPT, not the first
/// RTPS. Leaves the last outer corner's depth in SZ3 for the caller to capture.
/// Borrows the live scratch block for this call and reserves no storage.
static inline void _glowProjectFlameRingSegment(EffectBandScratch* block, s32 segmentIndex)
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

void glowDrawFlameRing(const GfxCoord* coord, s16 innerRadius, s32 width, s16 intensity)
{
    enum {
        GLOW_FLAME_RING_TRIG_SHIFT = 12,
        GLOW_FLAME_RING_STEP_SHIFT = 8,
    };

    EffectBandScratch* block;
    SVECTOR*           outerVertex;
    POLY_G4*           prim;
    s32                segmentIndex;
    s32                angle;
    s16                ringRadius;
    s16                outerRadius;

    outerRadius = innerRadius + width;
    block       = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    ringRadius = innerRadius;
    // Build concentric local XZ rings, then rotate and translate into the world.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        angle                           = segmentIndex << GLOW_FLAME_RING_STEP_SHIFT;
        block->topRing[segmentIndex].vx = (rsin(angle) * ringRadius) >> GLOW_FLAME_RING_TRIG_SHIFT;
        block->topRing[segmentIndex].vy = 0;
        block->topRing[segmentIndex].vz = (rcos(angle) * ringRadius) >> GLOW_FLAME_RING_TRIG_SHIFT;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&block->topRing[segmentIndex]);
        block->topRing[segmentIndex].vx   += coord->workm.t[0];
        block->topRing[segmentIndex].vy   += coord->workm.t[1];
        block->topRing[segmentIndex].vz   += coord->workm.t[2];
        block->bottomRing[segmentIndex].vx = (rsin(angle) * outerRadius) >> GLOW_FLAME_RING_TRIG_SHIFT;
        // Address the outer vertex through a byte view of the complete scratch block.
        outerVertex     = (SVECTOR*)((u8*)block + segmentIndex * sizeof(SVECTOR) + sizeof(block->topRing));
        outerVertex->vy = 0;
        outerVertex->vz = (rcos(angle) * outerRadius) >> GLOW_FLAME_RING_TRIG_SHIFT;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[segmentIndex]);
        block->bottomRing[segmentIndex].vx += coord->workm.t[0];
        outerVertex->vy                    += coord->workm.t[1];
        outerVertex->vz                    += coord->workm.t[2];
    }
    // Project and queue each segment separately; rejected segments emit no packets.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        _glowProjectFlameRingSegment(block, segmentIndex);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, intensity, intensity >> 1, intensity >> 2);
            setRGB1(prim, intensity, intensity >> 1, intensity >> 2);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sxy0.vx;
            prim->y0 = block->sxy0.vy;
            prim->x1 = block->sxy1.vx;
            prim->y1 = block->sxy1.vy;
            prim->x2 = block->sxy2.vx;
            prim->y2 = block->sxy2.vy;
            prim->x3 = block->sxy3.vx;
            prim->y3 = block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}
