/* Part of the glow drawing library; see glow_draw.h. */

/// Sets one halo segment's screen vertices and returns the next rim angle.
///
/// Borrows a writable quad and the projected centre with inner/outer pixel
/// radii. Rim angles use 4096 units per turn, zero down the screen; advances
/// by 256 units. Trigonometry uses Q12 factors. Writes only coordinates,
/// narrowing sums of raw projected halfwords and signed offsets to s16.
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

/// Draws an additive screen-facing halo with a black inner edge and a lit outer rim.
///
/// Projects the signed low halfwords of `coord`'s composed world origin.
/// Pixel radii are `(s16)innerRadiusScale * 64 / depth` and
/// `(s16)(innerRadiusScale + widthScale) * 64 / depth`; the sum is formed
/// before narrowing. Depth is camera Z / 4 plus one by default. Apobiosis
/// instead subtracts 64 and clamps to 16, using that depth for both radii and
/// sorting. Negative GTE flags reject the whole halo.
///
/// Borrows `coord` and three RGB bytes for the call. Reserves/releases one
/// `EffectShapeScratch` and queues sixteen Gouraud quads plus additive blend
/// commands in the current frame's arena. Coordinate rotation is not used.
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
#ifdef GLOW_DRAW_HALO_PULL
        // Pull both the sizing and sorting depth toward the eye.
        block->depth -= GLOW_DRAW_HALO_PULL;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef GLOW_DRAW_HALO_PULL
