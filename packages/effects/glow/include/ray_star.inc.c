/* Part of the glow drawing library; see glow.h. */

/// Draws a pulsing star glow at a point in a coordinate's local space, composing the coordinate first: eight
/// half-bright wedges with full-bright copies at half radius, and four half-bright rays whose tips alternate between
/// the outer radius and twice it. A point too near draws nothing.
///
/// The carrier colors vertex 2 of each packet through `GLOW_DRAW_RAY_STAR_OUTER` (the outer wedges, given half the
/// intensity), `GLOW_DRAW_RAY_STAR_INNER` (the inner ones, given it and its half) and `GLOW_DRAW_RAY_STAR_RAY` (the
/// rays). `GLOW_DRAW_RAY_STAR_EARLY_SIGNED_HALF_INTENSITY` and `GLOW_DRAW_RAY_STAR_UNSIGNED_RAY_INTENSITY` select the
/// halving the trailer coach and the breezeway use.
static void _glowDrawRayStar(GfxCoord* coord, const SVECTOR* localPoint, s16 pulseRate, s16 radiusScale)
{
    RoomGlowRadiiScratch* block;
    POLY_G4*              primitive;
    s32                   pulseSine;
    s32                   intensity;
    s32                   halfIntensity;
    s32                   angle;
    s32                   middleAngle;
    s32                   nextAngle;
    s32                   adjacentAngle;
#if GLOW_DRAW_RAY_STAR_EARLY_SIGNED_HALF_INTENSITY
    s32 intensityWork;
#endif

    actorRenderComposeCoord(coord);
    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);

    // Transform the local centre; unsigned additions wrap before halfword narrowing.
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(localPoint);
    gte_rtv0();
    gte_stsv(&block->worldPos);
    block->worldPos.vx += (u32)coord->workm.t[0];
    block->worldPos.vy += (u32)coord->workm.t[1];
    block->worldPos.vz += (u32)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= GLOW_MIN_DEPTH) {
        pulseSine          = rsin(gDisplayState.animFrame * pulseRate);
        angle              = 0;
        block->outerRadius = (radiusScale * GLOW_RADIUS_SCALE) / block->otz;
#if GLOW_DRAW_RAY_STAR_EARLY_SIGNED_HALF_INTENSITY
        // Keep the halving source separate from the saved full intensity;
        // its later scratch-byte-count use preserves the source register.
        intensityWork   = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        intensity       = intensityWork;
        intensityWork <<= 16;
        halfIntensity   = intensityWork >> 17;
#else
        intensity = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
#endif
        block->innerRadius = (radiusScale * GLOW_INNER_RADIUS_SCALE) / block->otz;
        // Eight wedge pairs form the disc; each inner copy has half the radius.
        do {
            primitive      = gGpuPrimCursor;
            gGpuPrimCursor = primitive + 1;
            setPolyG4(primitive);
#if !GLOW_DRAW_RAY_STAR_EARLY_SIGNED_HALF_INTENSITY
            halfIntensity = (s16)intensity >> 1;
#endif
            setRGB0(primitive, 0, 0, 0);
            setRGB1(primitive, 0, 0, 0);
            GLOW_DRAW_RAY_STAR_OUTER(primitive, halfIntensity);
            setRGB3(primitive, 0, 0, 0);
            primitive->x0 = block->screenPos.vx + ((block->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            middleAngle   = angle + GLOW_SIXTEENTH_TURN;
            primitive->y0 = block->screenPos.vy + ((block->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            primitive->x1 = block->screenPos.vx + ((block->outerRadius * rsin(middleAngle)) >> GLOW_TRIG_SHIFT);
            primitive->y1 = block->screenPos.vy + ((block->outerRadius * rcos(middleAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_EIGHTH_TURN;
            primitive->x2 = block->screenPos.vx;
            primitive->y2 = block->screenPos.vy;
            primitive->x3 = block->screenPos.vx + ((block->outerRadius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            primitive->y3 = block->screenPos.vy + ((block->outerRadius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                    primitive);
            gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, block->otz);

            primitive      = gGpuPrimCursor;
            gGpuPrimCursor = primitive + 1;
            setPolyG4(primitive);
            setRGB0(primitive, 0, 0, 0);
            setRGB1(primitive, 0, 0, 0);
            GLOW_DRAW_RAY_STAR_INNER(primitive, intensity, halfIntensity);
            setRGB3(primitive, 0, 0, 0);
            primitive->x0 = block->screenPos.vx + ((block->outerRadius * rsin(angle)) >> (GLOW_TRIG_SHIFT + 1));
            primitive->y0 = block->screenPos.vy + ((block->outerRadius * rcos(angle)) >> (GLOW_TRIG_SHIFT + 1));
            primitive->x1 = block->screenPos.vx + ((block->outerRadius * rsin(middleAngle)) >> (GLOW_TRIG_SHIFT + 1));
            primitive->y1 = block->screenPos.vy + ((block->outerRadius * rcos(middleAngle)) >> (GLOW_TRIG_SHIFT + 1));
            primitive->x2 = block->screenPos.vx;
            primitive->y2 = block->screenPos.vy;
            primitive->x3 = block->screenPos.vx + ((block->outerRadius * rsin(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            primitive->y3 = block->screenPos.vy + ((block->outerRadius * rcos(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                    primitive);
            gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);

#if GLOW_DRAW_RAY_STAR_EARLY_SIGNED_HALF_INTENSITY
        intensity = (s16)intensity >> 1;
#elif GLOW_DRAW_RAY_STAR_UNSIGNED_RAY_INTENSITY
        // Breezeway uses the unsigned low halfword for the ray colour.
        intensity = (u16)halfIntensity;
#else
        intensity = halfIntensity;
#endif
        // Overlay four diagonal rays, alternating their tip reach.
        angle = GLOW_EIGHTH_TURN;
        do {
            primitive      = gGpuPrimCursor;
            gGpuPrimCursor = primitive + 1;
            setPolyG4(primitive);
            setRGB0(primitive, 0, 0, 0);
            setRGB1(primitive, 0, 0, 0);
            GLOW_DRAW_RAY_STAR_RAY(primitive, intensity);
            setRGB3(primitive, 0, 0, 0);
            adjacentAngle = angle - GLOW_QUARTER_TURN;
            primitive->x0 = block->screenPos.vx + ((block->innerRadius * rsin(adjacentAngle)) >> (GLOW_TRIG_SHIFT + 1));
            primitive->y0 = block->screenPos.vy + ((block->innerRadius * rcos(adjacentAngle)) >> (GLOW_TRIG_SHIFT + 1));
            primitive->x1 = block->screenPos.vx + ((block->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            primitive->y1 = block->screenPos.vy + ((block->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            adjacentAngle = angle + GLOW_QUARTER_TURN;
            primitive->x2 = block->screenPos.vx;
            primitive->y2 = block->screenPos.vy;
            primitive->x3 = block->screenPos.vx + ((block->innerRadius * rsin(adjacentAngle)) >> (GLOW_TRIG_SHIFT + 1));
            primitive->y3 = block->screenPos.vy + ((block->innerRadius * rcos(adjacentAngle)) >> (GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                    primitive);
            gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, block->otz);

            primitive      = gGpuPrimCursor;
            gGpuPrimCursor = primitive + 1;
            setPolyG4(primitive);
            setRGB0(primitive, 0, 0, 0);
            setRGB1(primitive, 0, 0, 0);
            GLOW_DRAW_RAY_STAR_RAY(primitive, intensity);
            setRGB3(primitive, 0, 0, 0);
            primitive->x0 = block->screenPos.vx + ((block->innerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            primitive->y0 = block->screenPos.vy + ((block->innerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            primitive->x1 = block->screenPos.vx + ((block->outerRadius * rsin(adjacentAngle)) >> (GLOW_TRIG_SHIFT - 1));
            primitive->y1 = block->screenPos.vy + ((block->outerRadius * rcos(adjacentAngle)) >> (GLOW_TRIG_SHIFT - 1));
            adjacentAngle = angle + GLOW_HALF_TURN;
            primitive->x2 = block->screenPos.vx;
            primitive->y2 = block->screenPos.vy;
            primitive->x3 = block->screenPos.vx + ((block->innerRadius * rsin(adjacentAngle)) >> GLOW_TRIG_SHIFT);
            primitive->y3 = block->screenPos.vy + ((block->innerRadius * rcos(adjacentAngle)) >> GLOW_TRIG_SHIFT);
            angle         = adjacentAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                    primitive);
            gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);
    }
#if GLOW_DRAW_RAY_STAR_EARLY_SIGNED_HALF_INTENSITY
    intensityWork = sizeof(RoomGlowRadiiScratch);
    SCRATCH_STACK_RELEASE_BYTES_AT(SCRATCH_STACK_CURSOR_SLOT, intensityWork);
#else
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
#endif
}

#undef GLOW_DRAW_RAY_STAR_OUTER
#undef GLOW_DRAW_RAY_STAR_UNSIGNED_RAY_INTENSITY
#undef GLOW_DRAW_RAY_STAR_EARLY_SIGNED_HALF_INTENSITY
#undef GLOW_DRAW_RAY_STAR_INNER
#undef GLOW_DRAW_RAY_STAR_RAY
