/* Part of the library; see glutton.h. Inline helpers the fragments use. */

/// Replaces a coordinate's rotation with its current yaw at half scale.
///
/// Discards pitch, roll and the previous scale, preserving translation, parent
/// and stored angles; marks composition dirty. Requires a live writable
/// coordinate and initialized scratch with one `ActorScaleRotScratch` plus
/// nested axis-rotation capacity. All reservations are released before return.
static __inline__ void _actorRenderRescaleYawHalf(GfxCoord* coord)
{
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);

    yaw             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vx = ONE / 2;
    yawScratch->scale.vy = ONE / 2;
    yawScratch->scale.vz = ONE / 2;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);

    // Install only the rebuilt rotation, leaving translation intact.
    _actorRenderCopyRotation(coord, &yawScratch->rotation);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Replaces a coordinate's rotation with its current yaw at separate XZ and Y scales.
///
/// Both scales are signed Q12 (`ONE` is 1.0); X and Z take the s16 horizontal
/// scale and Y takes the s32 vertical scale. Pitch and roll are discarded.
/// Translation, parent and stored angles remain intact; composition is dirtied.
/// Requires a live writable coordinate and initialized scratch with one
/// `ActorScaleRotScratch` plus nested axis-rotation capacity. No pointer is retained.
static __inline__ void _actorRenderRescaleYawXZ(GfxCoord* coord, s16 horizontalScale, s32 verticalScale)
{
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);

    yaw             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vx = horizontalScale;
    yawScratch->scale.vy = verticalScale;
    yawScratch->scale.vz = horizontalScale;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);

    // Install only the rebuilt rotation, leaving translation intact.
    _actorRenderCopyRotation(coord, &yawScratch->rotation);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Horizontal gap from `coord` to the player's coordinate matrix `gPlayerStatus.coordMtx`, as an
/// `SVECTOR` the caller supplies.
static __inline__ void gluttonGapToCamera(GfxCoord* coord, SVECTOR* out)
{
    out->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    out->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    out->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
}

/// The first of the leading `count` contact records whose kind is 0x20000:
/// copies its point to `pos` and returns its key, or returns 0 when none is
/// found before an empty record or the end.
static __inline__ s32 _gluttonFindHit(SVECTOR* pos, WorldCollisionContact* records, s16 count)
{
    s16 i;

    for (i = 0; i < count; i++) {
        if (records[i].key.value == 0) {
            break;
        }
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

/// Scans a hit group's contacts for an attack and records its key and point
/// in `sc`. Returns the key, or 0 when nothing landed.
static __inline__ s32 _gluttonScanGroup(GluttonHitScratch* sc, GluttonHitGroup* group)
{
    s32 id;

    id            = _gluttonFindHit(&sc->contactPoint, group->contacts, ARRAY_SIZE(group->contacts));
    sc->attackKey = id;
    return id;
}

/// Spawns the impact effect for the attack recorded in `sc` on the group's
/// part. Returns whether the attack key is still set afterwards.
static __inline__ s32 _gluttonHitLanded(GluttonHitScratch* sc, GluttonHitGroup* group)
{
    _gluttonHitEffect(group->body.coord, sc->attackKey);
    return sc->attackKey != 0;
}
