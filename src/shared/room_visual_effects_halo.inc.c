/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Queues one additive animated mote square at a composed coordinate's world position.
///
/// `animationFrame & 3` selects a 24-texel cell. `packedHalfExtentRow` holds a
/// world-unit half-extent in bits 0..11 and a four-cell strip selector in bits
/// 12..15; callers use strips 0 and 1, both laid out along texture U at V = 0.
/// `packedBrightnessPalette` holds grey brightness in bits 0..7 and the palette
/// selector in bits 12..15 (0 uses the default CLUT); bits 8..11 are ignored.
/// Extent is scaled by 23 / (SZ3 / 4 + 1). Negative GTE flags suppress drawing.
/// Scratch storage is released after the draw; the packet lives for this frame.
static void _roomVisualEffectsDrawMote(const GfxCoord* coord, u16 animationFrame, u16 packedHalfExtentRow, u16 packedBrightnessPalette)
{
    enum { MOTE_SELECTOR_SHIFT       = 12,
           MOTE_BRIGHTNESS_MASK      = 0xFF,
           MOTE_FRAME_COUNT          = 4,
           MOTE_CELL_TEXELS          = 24,
           MOTE_PROJECTION_SCALE     = 23,
           MOTE_DEFAULT_PALETTE      = 0,
           MOTE_DEFAULT_CLUT_X       = 0xB0,
           MOTE_SELECTED_CLUT_BASE_X = 0xF0,
           MOTE_CLUT_Y               = 0x10B,
           MOTE_PALETTE_COLORS       = 16,
           MOTE_TEXTURE_PAGE_X       = 0x280 };

    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    DisplayState*        displayState;
    u16                  halfExtent;
    u16                  paletteIndex;
    u16                  textureRow;
    u16                  brightness;
    s32                  textureLeft;
    s32                  textureRight;
    s16                  screenEdge;

    // Unpack the sprite size and tint independently of their texture selectors.
    halfExtent                = packedHalfExtentRow;
    textureRow                = halfExtent >> MOTE_SELECTOR_SHIFT;
    halfExtent               &= ROOM_VISUAL_EFFECTS_MOTE_EXTENT;
    brightness                = packedBrightnessPalette;
    paletteIndex              = brightness >> MOTE_SELECTOR_SHIFT;
    brightness               &= MOTE_BRIGHTNESS_MASK;
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
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, ROOM_VISUAL_EFFECTS_TEXTURED_QUAD_BLEND);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, MOTE_TEXTURE_PAGE_X, 0);
        setRGB0(quad, brightness, brightness, brightness);
        if (paletteIndex != MOTE_DEFAULT_PALETTE) {
            quad->clut = getClut(paletteIndex * MOTE_PALETTE_COLORS + MOTE_SELECTED_CLUT_BASE_X, MOTE_CLUT_Y);
        } else {
            quad->clut = getClut(MOTE_DEFAULT_CLUT_X, MOTE_CLUT_Y);
        }
        textureLeft  = textureRow * (MOTE_FRAME_COUNT * MOTE_CELL_TEXELS) + (animationFrame & (MOTE_FRAME_COUNT - 1)) * MOTE_CELL_TEXELS;
        textureRight = textureLeft + MOTE_CELL_TEXELS - 1;
        setUV4(quad, textureLeft, 0, textureRight, 0, textureLeft, MOTE_CELL_TEXELS - 1, textureRight, MOTE_CELL_TEXELS - 1);
        projection->screenExtent = halfExtent * MOTE_PROJECTION_SCALE / projection->depth;
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
static void _roomVisualEffectsDrawHaloRing(const GfxCoord* coord, s32 blackRadius, s32 tintRadiusDelta, const u8 rgb[3])
{
    RoomFxRadialScratch* projection;
    POLY_G4*             quad;
    s32                  angle;
    s32                  nextAngle;
    s32                  tintRadius;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxRadialScratch);
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
        projection->radii.ring.black = ((s16)blackRadius * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / projection->depth;
        projection->radii.ring.tint  = ((s16)tintRadius * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / projection->depth;
        // Join the black and tinted edges in sixteenth-turn segments.
        for (angle = 0; angle < ROOM_VISUAL_EFFECTS_FULL_TURN; angle = nextAngle) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, rgb[0], rgb[1], rgb[2]);
            setRGB3(quad, rgb[0], rgb[1], rgb[2]);
            quad->x0  = projection->screenX + ((projection->radii.ring.black * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y0  = projection->screenY + ((projection->radii.ring.black * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            nextAngle = angle + 0x100;
            quad->x1  = projection->screenX + ((projection->radii.ring.black * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y1  = projection->screenY + ((projection->radii.ring.black * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x2  = projection->screenX + ((projection->radii.ring.tint * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y2  = projection->screenY + ((projection->radii.ring.tint * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x3  = projection->screenX + ((projection->radii.ring.tint * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y3  = projection->screenY + ((projection->radii.ring.tint * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxRadialScratch);
}

/// Draws an additive eight-wedge disc with a tinted centre and black rim.
///
/// `coord` must have a composed world matrix; `rgb` supplies three colour bytes.
/// The signed 16-bit radius in world units is scaled by 64 / (SZ3 / 4 + 1).
/// A negative GTE projection flag suppresses drawing.
static void _roomVisualEffectsDrawHaloDisc(const GfxCoord* coord, s16 radius, const u8 rgb[3])
{
    RoomFxFanScratch* projection;
    POLY_G4*          quad;
    s32               angle;
    s32               depth;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxFanScratch);
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
        depth              = projection->depth + 1;
        projection->depth  = depth;
        projection->radius = (radius * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / depth;

        // Each four-corner wedge shares the tinted centre and three black rim points.
        for (angle = 0; angle < ROOM_VISUAL_EFFECTS_FULL_TURN; angle += 0x200) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, rgb[0], rgb[1], rgb[2]);
            setRGB3(quad, 0, 0, 0);
            quad->x0 = projection->screenX + ((projection->radius * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y0 = projection->screenY + ((projection->radius * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x1 = projection->screenX + ((projection->radius * rsin(angle + 0x100)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y1 = projection->screenY + ((projection->radius * rcos(angle + 0x100)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x2 = projection->screenX;
            quad->y2 = projection->screenY;
            quad->x3 = projection->screenX + ((projection->radius * rsin(angle + 0x200)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y3 = projection->screenY + ((projection->radius * rcos(angle + 0x200)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxFanScratch);
}

/// Runs an expanding tinted halo and shrinking ring, followed by a fading star.
///
/// The task needs a coordinate body and an owned, zero-initialized `EffectWork`
/// in `spawnArg2.pointer`. Its borrowed parent must stay live; `pos` is the
/// local offset in game coordinate units. The signed halves of `spawnArg1`
/// supply a positive expansion duration (low) and a tint-row index 0..2 (high).
/// Initialization replaces that word with the remaining expansion ticks.
/// Brightness and disc radius grow by integer 256 / duration per active tick;
/// the star starts at brightness 255, fading by 16 while its radius grows by 32.
/// State 3 requests release. Nonzero room effect control below four pauses
/// every phase, including that request; four or above cancels immediately.
/// Completion releases the work allocation and task.
static inline void _roomVisualEffectsHaloTask(Task* task)
{
    /// Sets three colour bytes from the halo work's brightness and selected tint row.
    ///
    /// Both arguments are evaluated repeatedly and must have no side effects.
    /// The three-byte destination must not overlap the work; use as a standalone
    /// statement list with a terminating semicolon.
#define ROOM_VISUAL_EFFECTS_SET_HALO_TINT(rgb, work)                                     \
    (rgb)[0] = (work)->scale >> _roomVisualEffectsGetHaloShades()[(work)->index].rShift; \
    (rgb)[1] = (work)->scale >> _roomVisualEffectsGetHaloShades()[(work)->index].gShift; \
    (rgb)[2] = (work)->scale >> _roomVisualEffectsGetHaloShades()[(work)->index].bShift

    enum { HALO_INITIALIZE,
           HALO_EXPAND,
           HALO_FADE,
           HALO_RELEASE,
           HALO_EXPANSION_TARGET    = 0x100,
           HALO_ECHO_RADIUS_DELTA   = 0x100,
           HALO_INITIAL_RING_RADIUS = 0x300,
           HALO_RING_TINT_DELTA     = 0x80,
           HALO_FULL_BRIGHTNESS     = 0xFF,
           HALO_FADE_STEP           = 0x10,
           HALO_STAR_RADIUS_SCALE   = 4,
           HALO_FADE_RADIUS_STEP    = 8 };

    u8          rgb[3];
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         tintIndex;

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
    switch (task->state) {
        case HALO_INITIALIZE:
            // Attach at the spawn offset and unpack the tint and expansion duration.
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = work->pos.vx;
            coord->coord.t[1]   = work->pos.vy;
            coord->coord.t[2]   = work->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            tintIndex             = task->spawnArg1.halves.high;
            work->index           = tintIndex;
            task->spawnArg1.value = task->spawnArg1.halves.low;
            task->state           = HALO_EXPAND;
            work->step            = HALO_EXPANSION_TARGET / task->spawnArg1.value;
            return;
        case HALO_EXPAND:
            // scale holds brightness; angle holds the radius in world units.
            actorRenderComposeCoord(coord);
            work->scale           += work->step;
            work->angle           += work->step;
            task->spawnArg1.value -= 1;
            ROOM_VISUAL_EFFECTS_SET_HALO_TINT(rgb, work);
            _roomVisualEffectsDrawHaloDisc(coord, work->angle, rgb);
            rgb[0] = rgb[0] >> 1;
            rgb[1] = rgb[1] >> 1;
            rgb[2] = rgb[2] >> 1;
            if (work->age & 1) {
                _roomVisualEffectsDrawHaloDisc(coord, (s16)(work->angle + HALO_ECHO_RADIUS_DELTA), rgb);
            }
            _roomVisualEffectsDrawHaloRing(coord, (s16)(HALO_INITIAL_RING_RADIUS - (u16)work->angle * 2), HALO_RING_TINT_DELTA, rgb);
            if (task->spawnArg1.value == 0) {
                work->scale = HALO_FULL_BRIGHTNESS;
                task->state = HALO_FADE;
                return;
            }
            return;
        case HALO_FADE:
            // Replace the disc and ring with a larger star until its tint is dark.
            actorRenderComposeCoord(coord);
            if (work->scale >= HALO_FADE_STEP + 1) {
                ROOM_VISUAL_EFFECTS_SET_HALO_TINT(rgb, work);
                _roomVisualEffectsDrawHaloStar(coord, (u16)work->angle * HALO_STAR_RADIUS_SCALE, rgb);
                work->scale -= HALO_FADE_STEP;
                work->angle += HALO_FADE_RADIUS_STEP;
                return;
            }
            /* fallthrough */
        case HALO_RELEASE:
            break;
        default:
            return;
    }
    effectKillTask(work, task);
}
#undef ROOM_VISUAL_EFFECTS_SET_HALO_TINT

/// Runs the halo-section orange burst: growing disc and layered glow inside a fading ring.
///
/// The task's extra body supplies its coordinate; `spawnArg2.pointer` owns an
/// `EffectWork`, released on completion or cancellation. Central and ring
/// brightness start at 224, with glow half-extent and ring base radius at 128
/// world units. Each active tick grows the glow by 16 and draws the disc at
/// twice its half-extent. The ring's black edge is 3/2 of its base radius and
/// its tinted edge is 96 units farther out: it grows by 72 and fades by 24
/// per tick. Once the ring is dark, central brightness falls by 24 per tick.
/// Nonzero room effect control pauses the task; four or above cancels it.
static inline void _roomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    /// Sets the burst's orange tint at full, half and quarter channel brightness.
    ///
    /// Both arguments are evaluated repeatedly and must have no side effects;
    /// `level` must not alias the three-byte destination. Use only as a
    /// standalone statement list with a terminating semicolon.
#define ROOM_VISUAL_EFFECTS_SET_HALO_BURST_TINT(rgb, level) \
    (rgb)[0] = (level);                                     \
    (rgb)[1] = (level) >> 1;                                \
    (rgb)[2] = (level) >> 2

    enum { BURST_INITIALIZE,
           BURST_EXPAND,
           BURST_INITIAL_LEVEL       = 0xE0,
           BURST_INITIAL_HALF_EXTENT = 0x80,
           BURST_GLOW_EXTENT_STEP    = 0x10,
           BURST_RING_BASE_STEP      = 0x30,
           BURST_RING_TINT_DELTA     = 0x60,
           BURST_LEVEL_STEP          = 0x18 };

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
        // scale/period hold centre/ring brightness; angle/step hold glow extent/ring base radius.
        work->age    = 1;
        work->scale  = BURST_INITIAL_LEVEL;
        work->angle  = BURST_INITIAL_HALF_EXTENT;
        work->period = BURST_INITIAL_LEVEL;
        work->step   = BURST_INITIAL_HALF_EXTENT;
        task->state  = BURST_EXPAND;
    }
    actorRenderComposeCoord(coord);
    ROOM_VISUAL_EFFECTS_SET_HALO_BURST_TINT(rgb, work->scale);
    glowHalfExtent = work->angle + BURST_GLOW_EXTENT_STEP;
    work->angle    = glowHalfExtent;
    _roomVisualEffectsDrawHaloDisc(coord, (s16)(glowHalfExtent * 2), rgb);
    _roomVisualEffectsDrawHaloBurstGlow(coord, work->angle);
    // Fade the expanding ring before reducing the central burst brightness.
    if (work->period > BURST_LEVEL_STEP) {
        ROOM_VISUAL_EFFECTS_SET_HALO_BURST_TINT(rgb, work->period);
        _roomVisualEffectsDrawHaloRing(coord, (s16)(work->step * 3 / 2), BURST_RING_TINT_DELTA, rgb);
        work->period -= BURST_LEVEL_STEP;
        work->step   += BURST_RING_BASE_STEP;
        return;
    }
    work->scale -= BURST_LEVEL_STEP;
    if (work->scale < BURST_LEVEL_STEP) {
        effectKillTask(work, task);
    }
#undef ROOM_VISUAL_EFFECTS_SET_HALO_BURST_TINT
}
