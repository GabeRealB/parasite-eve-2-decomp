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

/// Writes the three-axis offset from a coordinate to the live player.
///
/// Both translations must use the same parent coordinate frame. Components
/// use world units and narrow to signed halfwords; `playerOffset->pad` is
/// untouched. The coordinate and player matrix are borrowed only for the call.
static __inline__ void _gluttonGetPlayerOffset(const GfxCoord* sourceCoord, SVECTOR* playerOffset)
{
    playerOffset->vx = gPlayerStatus.coordMtx->t[0] - sourceCoord->coord.t[0];
    playerOffset->vy = gPlayerStatus.coordMtx->t[1] - sourceCoord->coord.t[1];
    playerOffset->vz = gPlayerStatus.coordMtx->t[2] - sourceCoord->coord.t[2];
}

/// Returns the first attack key in a contact-table prefix and copies its point.
///
/// `contactCount` counts elements, from 0 to 32767, in a readable table.
/// A zero key ends the scan even if later entries are occupied. A miss returns
/// zero and leaves the output untouched; a hit copies the three world-position
/// components, leaving `pad` untouched. Neither pointer is retained.
static __inline__ s32 _gluttonFindHit(SVECTOR* contactPointOut, const WorldCollisionContact* contacts, s16 contactCount)
{
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            contactPointOut->vx = contacts[contactIndex].point.vx;
            contactPointOut->vy = contacts[contactIndex].point.vy;
            contactPointOut->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}

/// Records the first attack in a hit group's five contacts and returns its key.
///
/// A miss clears `scratch->attackKey` to zero and leaves its contact point
/// unchanged. Requires a writable scratch block and a readable group; neither
/// pointer is retained.
static __inline__ s32 _gluttonScanGroup(GluttonHitScratch* scratch, const GluttonHitGroup* group)
{
    s32 attackKey;

    attackKey          = _gluttonFindHit(&scratch->contactPoint, group->contacts, ARRAY_SIZE(group->contacts));
    scratch->attackKey = attackKey;
    return attackKey;
}

/// Spawns the recorded attack's impact effect on the hit group's coordinate.
///
/// Requires a nonzero player attack key, a live group coordinate and scratch
/// capacity for the nested effect call. Returns whether the scratch key is
/// nonzero after that call; it does not search for another contact.
static __inline__ s32 _gluttonSpawnGroupHitEffect(GluttonHitScratch* scratch, const GluttonHitGroup* group)
{
    _gluttonHitEffect(group->body.coord, scratch->attackKey);
    return scratch->attackKey != 0;
}
