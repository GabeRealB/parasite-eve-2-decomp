/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Stores an endpoint snapshot in world space, independent of its moving anchor.
///
/// `endpoint->workm` and `gGfxViewCoord.workm` must be current transforms into
/// the same view space, with an orthonormal view rotation. The distinct,
/// word-aligned coordinates remain caller-owned. Copies the view-space cache
/// and removes the view transform to obtain a world-space local matrix,
/// parented to the persistent `gGfxViewCoord`.
///
/// Leaves `composeStamp` and `param` untouched: the caller must mark the frame
/// dirty before composing it again. Retains no endpoint pointer. Loads the
/// endpoint's GTE rotation and translation before rebasing; requires 48 free
/// scratch-stack bytes, released before return.
static inline void _roomVisualEffectsStoreTrailFrame(GfxCoord* historyFrame, const GfxCoord* endpoint)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = endpoint->workm;
    gte_SetRotMatrix(&endpoint->workm);
    gte_SetTransMatrix(&endpoint->workm);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &historyFrame->workm, &historyFrame->coord);
}

/// Draws an additive sixteen-segment ring around a composed coordinate's world position.
///
/// `blackRadius` and `blackRadius + tintRadiusDelta` are narrowed separately to
/// signed 16-bit world units, then scaled by 64 / (SZ3 / 4 + 1). The first edge
/// is black and the second has the three-byte `rgb` tint; a negative delta can
/// reverse their radial order. A negative GTE projection flag suppresses drawing.
static void _roomVisualEffectsDrawFlashRing(const GfxCoord* coord, s32 blackRadius, s32 tintRadiusDelta, const u8 rgb[3])
{
    RoomFxFlashRingScratch* projection;
    POLY_G4*                quad;
    s32                     angle;
    s32                     nextAngle;
    s16                     blackRadius16 = blackRadius;
    s16                     tintRadius16  = blackRadius + tintRadiusDelta;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxFlashRingScratch);
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
        projection->radii.black = (blackRadius16 * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / projection->depth;
        projection->radii.tint  = (tintRadius16 * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / projection->depth;

        // One quad per sixteenth of a turn, joining the black edge to the tinted edge.
        angle = 0;
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, rgb[0], rgb[1], rgb[2]);
            setRGB3(quad, rgb[0], rgb[1], rgb[2]);
            quad->x0  = projection->screenX + ((projection->radii.black * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y0  = projection->screenY + ((projection->radii.black * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            nextAngle = angle + 0x100;
            quad->x1  = projection->screenX + ((projection->radii.black * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y1  = projection->screenY + ((projection->radii.black * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x2  = projection->screenX + ((projection->radii.tint * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y2  = projection->screenY + ((projection->radii.tint * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x3  = projection->screenX + ((projection->radii.tint * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y3  = projection->screenY + ((projection->radii.tint * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            angle     = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);
        } while (angle < ROOM_VISUAL_EFFECTS_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxFlashRingScratch);
}

/// Draws an additive eight-wedge disc with a tinted centre and black rim.
///
/// `coord` must have a composed world matrix; `rgb` supplies three colour bytes.
/// The signed 16-bit radius in world units is scaled by 64 / (SZ3 / 4 + 1).
/// A negative GTE projection flag suppresses drawing.
static void _roomVisualEffectsDrawFlashDisc(const GfxCoord* coord, s16 radius, const u8 rgb[3])
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
    SCRATCH_STACK_RELEASE_BYTES(sizeof(RoomFxFanScratch));
}
