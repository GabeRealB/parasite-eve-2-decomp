/* Part of the glow drawing library; see glow.h. */

/// Sets a halo segment's corners between the inner and outer radii, from `angle` to a sixteenth turn on, and returns
/// that next angle.
static inline s32 _glowSetHaloQuadVertices(POLY_G4* quad, const EffectShapeScratch* projection, s32 angle)
{
    s32 nextAngle;

    quad->x0  = projection->screenX + ((projection->extent.ring.inner * rsin(angle)) >> GLOW_TRIG_SHIFT);
    quad->y0  = projection->screenY + ((projection->extent.ring.inner * rcos(angle)) >> GLOW_TRIG_SHIFT);
    nextAngle = angle + GLOW_SIXTEENTH_TURN;
    quad->x1  = projection->screenX + ((projection->extent.ring.inner * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
    quad->y1  = projection->screenY + ((projection->extent.ring.inner * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
    quad->x2  = projection->screenX + ((projection->extent.ring.outer * rsin(angle)) >> GLOW_TRIG_SHIFT);
    quad->y2  = projection->screenY + ((projection->extent.ring.outer * rcos(angle)) >> GLOW_TRIG_SHIFT);
    quad->x3  = projection->screenX + ((projection->extent.ring.outer * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
    quad->y3  = projection->screenY + ((projection->extent.ring.outer * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
    return nextAngle;
}

/// Draws an additive ring facing the screen at a coordinate's origin, black at `innerRadiusScale` and lit with `rgb`
/// `widthScale` further out, both in perspective. `GLOW_DRAW_HALO_DEPTH_PULL` draws it that much nearer.
static void _glowDrawHalo(const GfxCoord* coord, s32 innerRadiusScale, s32 widthScale, const u8 rgb[3])
{
    EffectShapeScratch* block;
    POLY_G4*            quad;
    s32                 angle;
    s32                 nextAngle;
    s32                 outerRadiusScale;

    // Project the centre once; the ring is constructed entirely in screen space.
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    outerRadiusScale     = innerRadiusScale + widthScale;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
#ifdef GLOW_DRAW_HALO_DEPTH_PULL
        // Pull both the sizing and sorting depth toward the eye.
        block->depth -= GLOW_DRAW_HALO_DEPTH_PULL;
        if (block->depth < GLOW_NEAR_DEPTH_CLAMP) {
            block->depth = GLOW_NEAR_DEPTH_CLAMP;
        }
#else
        block->depth++;
#endif
        block->extent.ring.inner = ((s16)innerRadiusScale * GLOW_RADIUS_SCALE) / block->depth;
        block->extent.ring.outer = ((s16)outerRadiusScale * GLOW_RADIUS_SCALE) / block->depth;
        for (angle = 0; angle < GLOW_FULL_TURN; angle = nextAngle) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, rgb[0], rgb[1], rgb[2]);
            setRGB3(quad, rgb[0], rgb[1], rgb[2]);
            nextAngle = _glowSetHaloQuadVertices(quad, block, angle);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef GLOW_DRAW_HALO_DEPTH_PULL
