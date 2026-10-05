/* Part of the glow drawing library; see glow_draw.h. */

/// Places a fan blade centre and its two dark rim vertices in screen pixels.
static inline void _glowSetWedgeVertices(POLY_G3* prim, EffectCentreScratch* block, s32 angle)
{
    enum { GLOW_WEDGE_TRIG_SHIFT = 12,
           GLOW_WEDGE_HALF_ANGLE = 0x20 };
    s32 rimAngle;
    s32 firstRimAngle;

    rimAngle      = (s16)angle;
    firstRimAngle = rimAngle - GLOW_WEDGE_HALF_ANGLE;
    prim->x0      = block->screenX;
    prim->y0      = block->screenY;
    prim->x1      = block->screenX + ((block->screenExtent * rsin(firstRimAngle)) >> GLOW_WEDGE_TRIG_SHIFT);
    prim->y1      = block->screenY + ((block->screenExtent * rcos(firstRimAngle)) >> GLOW_WEDGE_TRIG_SHIFT);
    rimAngle     += GLOW_WEDGE_HALF_ANGLE;
    prim->x2      = block->screenX + ((block->screenExtent * rsin(rimAngle)) >> GLOW_WEDGE_TRIG_SHIFT);
    prim->y2      = block->screenY + ((block->screenExtent * rcos(rimAngle)) >> GLOW_WEDGE_TRIG_SHIFT);
}

void glowDrawWedge(const GfxCoord* coord, s32 radiusScale, s32 angle, const u8 rgb[3])
{
    enum {
        GLOW_WEDGE_RADIUS_SCALE = 128,
    };

    EffectCentreScratch* stackTop;
    EffectCentreScratch* block;
    SVECTOR*             worldPoint;
    POLY_G3*             prim;
    u16                  worldZ;

    // Stage the narrowed world origin below the saved scratch cursor.
    stackTop                                  = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (stackTop - 1)->worldPoint.vx             = (u16)coord->workm.t[0];
    block                                     = stackTop - 1;
    block->worldPoint.vy                      = (u16)coord->workm.t[1];
    worldZ                                    = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = block;
    block->worldPoint.vz                      = worldZ;
    worldPoint                                = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(stackTop - 1)->screenX);
    gte_stflg(&(stackTop - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(stackTop - 1)->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        setRGB0(prim, rgb[0], rgb[1], rgb[2]);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, 0, 0, 0);
        block->screenExtent = ((s16)radiusScale * GLOW_WEDGE_RADIUS_SCALE) / block->depth;
        _glowSetWedgeVertices(prim, block, angle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
