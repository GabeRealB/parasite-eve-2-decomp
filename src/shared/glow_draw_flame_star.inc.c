/* Part of the glow drawing library; see glow_draw.h. */

/// Reserves a Gouraud quad with a coloured centre and a black rim.
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
