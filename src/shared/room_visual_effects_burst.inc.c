/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Draws one additive animated spark square at a composed coordinate's world position.
///
/// `animationFrame & 3` selects a 24-texel cell. The low signed 16 bits of
/// `halfExtent`, in world units, are scaled by 23 / (SZ3 / 4 + 1); the low byte
/// of `brightness` tints all three channels. Negative projection flags suppress
/// drawing. Scratch storage and the emitted GPU packet live only for this draw
/// and the current frame, respectively.
static void _roomVisualEffectsDrawFlyingSpark(const GfxCoord* coord, s32 animationFrame, s32 halfExtent, s32 brightness)
{
    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    DisplayState*        displayState;
    s32                  extent16;
    s32                  textureU;
    s32                  scaledExtent;
    s16                  screenEdge;
    u16                  worldZ;

    scratchEnd                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    scratchEnd[-1].worldPoint.vx              = coord->workm.t[0];
    projection                                = scratchEnd - 1;
    projection->worldPoint.vy                 = coord->workm.t[1];
    worldZ                                    = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = projection;
    projection->worldPoint.vz                 = worldZ;
    worldPoint                                = &projection->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].screenX);
    gte_stflg(&scratchEnd[-1].projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&scratchEnd[-1].depth);
        projection->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, ROOM_VISUAL_EFFECTS_TEXTURED_QUAD_BLEND);
        quad->tpage = 0x2A;
        quad->clut  = 0x42CB;
        textureU    = (animationFrame & 3) * 24;
        setRGB0(quad, brightness, brightness, brightness);
        setUVWH(quad, textureU + 0x60, 0, 0x17, 0x17);
        extent16                 = (s16)halfExtent;
        scaledExtent             = extent16 * 24;
        projection->screenExtent = (scaledExtent - extent16) / projection->depth;
        screenEdge               = projection->screenX - projection->screenExtent;
        quad->x2                 = screenEdge;
        quad->x0                 = screenEdge;
        screenEdge               = projection->screenX + projection->screenExtent;
        quad->x3                 = screenEdge;
        quad->x1                 = screenEdge;
        screenEdge               = projection->screenY - projection->screenExtent;
        quad->y1                 = screenEdge;
        quad->y0                 = screenEdge;
        screenEdge               = projection->screenY + projection->screenExtent;
        quad->y3                 = screenEdge;
        quad->y2                 = screenEdge;
        displayState             = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << displayState->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Draws an additive sixteen-segment ring around a composed coordinate's world position.
///
/// `blackRadius` and `blackRadius + tintRadiusDelta` are narrowed separately to
/// signed 16-bit world units, then scaled by 64 / (SZ3 / 4 + 1). The first edge
/// is black and the second has the three-byte `rgb` tint; a negative delta can
/// reverse their radial order. A negative GTE projection flag suppresses drawing.
static void _roomVisualEffectsDrawFlyingRing(const GfxCoord* coord, s32 blackRadius, s32 tintRadiusDelta, const u8 rgb[3])
{
    EffectShapeScratch* projection;
    POLY_G4*            quad;
    s32                 angle;
    s32                 nextAngle;
    s32                 tintRadius;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    tintRadius                = blackRadius + tintRadiusDelta;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        projection->depth++;
        projection->extent.ring.inner = ((s16)blackRadius * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / projection->depth;
        projection->extent.ring.outer = ((s16)tintRadius * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / projection->depth;
        // Join the black and tinted edges in sixteenth-turn segments.
        for (angle = 0; angle < ROOM_VISUAL_EFFECTS_FULL_TURN; angle = nextAngle) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, rgb[0], rgb[1], rgb[2]);
            setRGB3(quad, rgb[0], rgb[1], rgb[2]);
            quad->x0  = projection->screenX + ((projection->extent.ring.inner * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y0  = projection->screenY + ((projection->extent.ring.inner * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            nextAngle = angle + 0x100;
            quad->x1  = projection->screenX + ((projection->extent.ring.inner * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y1  = projection->screenY + ((projection->extent.ring.inner * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x2  = projection->screenX + ((projection->extent.ring.outer * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y2  = projection->screenY + ((projection->extent.ring.outer * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x3  = projection->screenX + ((projection->extent.ring.outer * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y3  = projection->screenY + ((projection->extent.ring.outer * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws an additive eight-wedge disc with a tinted centre and black rim.
///
/// `coord` must have a composed world matrix; `rgb` supplies three colour bytes.
/// The low signed 16 bits of `radius`, in world units, are scaled by
/// 64 / (SZ3 / 4 + 1).
/// A negative GTE projection flag suppresses drawing.
static void _roomVisualEffectsDrawFlyingDisc(const GfxCoord* coord, s32 radius, const u8 rgb[3])
{
    EffectCentreScratch* projection;
    POLY_G4*             quad;
    s32                  angle;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        projection->depth++;
        projection->screenExtent = ((s16)radius * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / projection->depth;
        // Each four-corner wedge shares the tinted centre and three black rim points.
        for (angle = 0; angle < ROOM_VISUAL_EFFECTS_FULL_TURN; angle += 0x200) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, rgb[0], rgb[1], rgb[2]);
            setRGB3(quad, 0, 0, 0);
            quad->x0 = projection->screenX + ((projection->screenExtent * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y0 = projection->screenY + ((projection->screenExtent * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x1 = projection->screenX + ((projection->screenExtent * rsin(angle + 0x100)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y1 = projection->screenY + ((projection->screenExtent * rcos(angle + 0x100)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x2 = projection->screenX;
            quad->y2 = projection->screenY;
            quad->x3 = projection->screenX + ((projection->screenExtent * rsin(angle + 0x200)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y3 = projection->screenY + ((projection->screenExtent * rcos(angle + 0x200)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Runs the flying-section orange burst with an expanding disc, layered glow and fading ring.
///
/// The task's extra body supplies its coordinate and `spawnArg2` owns an
/// `EffectWork`. Brightness starts at 224 and glow half-extent at 128 world units.
/// The glow grows by 16 each active tick. The ring's black edge grows by 72 while
/// fading by 24; after that phase the central brightness falls by 24 until the
/// work is released. Room effect control pauses at nonzero and cancels at four
/// or above. This section keeps its own emitted drawer instances.
static inline void _roomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    enum { BURST_INITIALIZE,
           BURST_EXPAND,
           BURST_LEVEL_STEP = 0x18 };

    u8          rgb[3];
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s16         glowHalfExtent;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    work->age++;
    if (task->state == BURST_INITIALIZE) {
        work->age    = 1;
        work->scale  = 0xE0;
        work->angle  = 0x80;
        work->period = 0xE0;
        work->step   = 0x80;
        task->state  = BURST_EXPAND;
    }
    actorRenderComposeCoord(coord);
    rgb[0]         = work->scale;
    rgb[1]         = work->scale >> 1;
    rgb[2]         = work->scale >> 2;
    glowHalfExtent = work->angle + 0x10;
    work->angle    = glowHalfExtent;
    _roomVisualEffectsDrawFlyingDisc(coord, (s16)(glowHalfExtent * 2), rgb);
    _roomVisualEffectsDrawFlyingBurstGlow(coord, work->angle);
    // Fade the expanding ring before reducing the central burst brightness.
    if (work->period > BURST_LEVEL_STEP) {
        rgb[0] = work->period;
        rgb[1] = work->period >> 1;
        rgb[2] = work->period >> 2;
        _roomVisualEffectsDrawFlyingRing(coord, (s16)(work->step * 3 / 2), 0x60, rgb);
        work->period -= BURST_LEVEL_STEP;
        work->step   += 0x30;
        return;
    }
    work->scale -= BURST_LEVEL_STEP;
    if (work->scale < BURST_LEVEL_STEP) {
        effectKillTask(work, task);
    }
}
