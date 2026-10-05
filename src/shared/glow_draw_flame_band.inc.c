/* Part of the glow drawing library; see glow_draw.h. */

/// Projects the four corners of one flame segment into its scratch record.
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}
