/* A pulsing flare: one frame of a light that pulses between black and red or a
 * second color, drawn as a round fan with four rays or as a flat diamond with
 * optional streaks. The Akropolis square's telephone and the roof garden's
 * lights carry it as their room-effect task, which releases its work block once
 * queued, so the room respawns it every frame it wants the light. The fire
 * escape's lights are an earlier copy that diverged (its diamond's height is the
 * inner radius, and its streaks queue the last quad again), and keep their own.
 *
 * The task's spawn options pack the pulse rate in bits 0..7, the radius in
 * 8..15, the second color in bit 16, streaks in bit 28 and the round fan in
 * bit 31.
 *
 * The carrier defines, before including it at the function's position:
 *   PULSING_FLARE_TASK                    the task's name, which gameplay's room-effect table names
 *   PULSING_FLARE_SINE_SAMPLES            its sine table; the first ray reads the two halfwords before it
 *   PULSING_FLARE_BLUE(level)             the second color's blue: 0 for green, `level` for cyan
 *   PULSING_FLARE_OUTER_RADIUS(scale, options)
 *   PULSING_FLARE_INNER_RADIUS(scale, options)
 *                                         the fan's radii before the depth divide, from the radius field
 *                                         and the spawn options
 * Requires glow_draw.h.
 */

/// Initializes a flare quad: black corners around a center of `red` and the second color's `level`.
#define _PULSING_FLARE_INIT_QUAD(quad, red, level)                      \
    ((void)(setPolyG4((quad)),                                          \
            setRGB0((quad), 0, 0, 0),                                   \
            setRGB1((quad), 0, 0, 0),                                   \
            setRGB2((quad), (red), (level), PULSING_FLARE_BLUE(level)), \
            setRGB3((quad), 0, 0, 0)))

/// Initializes a two-segment flare streak fading from its center to black.
static inline void _pulsingFlareInitStreak(LINE_G3* streak, u8 centerRed, u8 centerLevel)
{
    setLineG3(streak);
    setRGB0(streak, 0, 0, 0);
    setRGB1(streak, centerRed, centerLevel, PULSING_FLARE_BLUE(centerLevel));
    setRGB2(streak, 0, 0, 0);
}

/// Queues a flare packet and prepends its additive draw mode at the same depth.
static inline void _pulsingFlareQueue(void* primitive, const s32* depthWord)
{
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(*depthWord),
            primitive);
    // Prepending the draw mode after the primitive makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, *depthWord);
}

void PULSING_FLARE_TASK(Task* task)
{
    enum {
        PULSING_FLARE_PULSE_RATE_MASK      = 0xFF,
        PULSING_FLARE_RADIUS_SHIFT         = 8,
        PULSING_FLARE_RADIUS_MASK          = 0xFF,
        PULSING_FLARE_SECOND_COLOR_SHIFT   = 16,
        PULSING_FLARE_STREAKS              = 0x10000000,
        PULSING_FLARE_PULSE_FALLING        = 0x80,
        PULSING_FLARE_PULSE_LEVEL_MASK     = 0x7F,
        PULSING_FLARE_DIAMOND_RADIUS_SHIFT = 9,
        PULSING_FLARE_CIRCLE_STEPS         = 16,
        PULSING_FLARE_DEPTH_TO_TAG_SHIFT   = 4,
        PULSING_FLARE_DEPTH_TAG_MASK       = GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt),
    };
    GfxCoord*             coord;
    EffectWork*           work;
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    s32                   partIndex;
    s32                   pulsePhase;
    s32                   pulseOrOptions; // Folded pulse level, then the packed spawn options
    s32                   radiusScale;
    s16                   intensity;
    s16                   secondColor;
    s16                   halfIntensity;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    // Draw one projected frame before retiring the counted work and task.
    actorRenderComposeCoord(coord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = coord->workm.t[0];
    projection->worldPos.vy = coord->workm.t[1];
    projection->worldPos.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= GLOW_MIN_DEPTH) {
        // Fold the phase into a 0..127 triangle wave, then select red or the second color.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & PULSING_FLARE_PULSE_RATE_MASK;
        secondColor = (task->spawnArg1.value >> PULSING_FLARE_SECOND_COLOR_SHIFT) & 1;
        if (pulsePhase & PULSING_FLARE_PULSE_FALLING) {
            pulseOrOptions = ~pulsePhase & PULSING_FLARE_PULSE_LEVEL_MASK;
        } else {
            pulseOrOptions = pulsePhase & PULSING_FLARE_PULSE_LEVEL_MASK;
        }
        intensity      = pulseOrOptions * 2;
        pulseOrOptions = task->spawnArg1.value;
        if (pulseOrOptions < 0) {
            // Layer two concentric wedge fans and four half-bright rays.
            radiusScale             = (pulseOrOptions >> PULSING_FLARE_RADIUS_SHIFT) & PULSING_FLARE_RADIUS_MASK;
            projection->outerRadius = PULSING_FLARE_OUTER_RADIUS(radiusScale, task->spawnArg1.value) / projection->otz;
            projection->innerRadius = PULSING_FLARE_INNER_RADIUS(radiusScale, task->spawnArg1.value) / projection->otz;
            for (partIndex = 0; partIndex < PULSING_FLARE_CIRCLE_STEPS; partIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                _PULSING_FLARE_INIT_QUAD(quad, (intensity * (secondColor ^ 1)) >> 1, (secondColor * intensity) >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 2]) >> GLOW_TRIG_SHIFT);
                _pulsingFlareQueue(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                _PULSING_FLARE_INIT_QUAD(quad, intensity * (secondColor ^ 1), secondColor * intensity);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _pulsingFlareQueue(quad, &projection->otz);
            }
            // The first ray reads sine index -2: the halfword before the table, which is the carrier's.
            halfIntensity = intensity >> 1;
            for (partIndex = 2; partIndex < PULSING_FLARE_CIRCLE_STEPS; partIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                _PULSING_FLARE_INIT_QUAD(quad, halfIntensity * (secondColor ^ 1), secondColor * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex - 4]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 4]) >> (GLOW_TRIG_SHIFT - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex]) >> (GLOW_TRIG_SHIFT - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                _pulsingFlareQueue(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                _PULSING_FLARE_INIT_QUAD(quad, halfIntensity * (secondColor ^ 1), secondColor * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 12]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * PULSING_FLARE_SINE_SAMPLES[partIndex + 8]) >> (GLOW_TRIG_SHIFT + 1));
                _pulsingFlareQueue(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((pulseOrOptions >> PULSING_FLARE_RADIUS_SHIFT) & PULSING_FLARE_RADIUS_MASK) << PULSING_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < 2; partIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                _PULSING_FLARE_INIT_QUAD(quad, intensity * (secondColor ^ 1), secondColor * intensity);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->outerRadius) + projection->outerRadius * (partIndex + partIndex);
                addPrim((&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> PULSING_FLARE_DEPTH_TO_TAG_SHIFT & PULSING_FLARE_DEPTH_TAG_MASK]),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz);
            }
            // Optional streaks cross at the diamond center.
            if (task->spawnArg1.value & PULSING_FLARE_STREAKS) {
                for (partIndex = 0; partIndex < 2; partIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    _pulsingFlareInitStreak(streak, intensity * (secondColor ^ 1), secondColor * intensity);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (partIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->outerRadius * (partIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (partIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->outerRadius * (partIndex + 1);
                    _pulsingFlareQueue(streak, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    effectKillTask(work, task);
}

#undef _PULSING_FLARE_INIT_QUAD
