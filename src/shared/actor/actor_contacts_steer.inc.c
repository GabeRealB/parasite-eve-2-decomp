/* Part of the actor contacts library; see actor_contacts.h. */

/// Applies one horizontal avoidance step and adds it to the running correction.
///
/// `parentYaw` uses 4096 units per turn, from +Z toward +X in `coord`'s
/// parent frame. `signedDistance` uses game-coordinate units; a negative value
/// moves opposite that yaw. The normalized axis has 12 fractional bits, and
/// the step retains the GTE's 12-bit product shift and signed-halfword saturation.
///
/// Borrows the caller's live `steerScratch` block, overwriting only `rot.m`
/// and `dir`'s XYZ components; neither needs initialized contents.
/// The block, `coord` and `pushDelta` must be disjoint.
/// `pushDelta` must already hold the running correction: XZ additions narrow
/// to signed halfwords, while the coordinate's XZ translation stays 32-bit.
/// Both Y components and `pushDelta->pad` are untouched. The caller must mark
/// the changed coordinate dirty before composing its cached matrix again.
///
/// Borrows all storage for this call. The yaw builder requires an initialized
/// scratch stack; this helper reserves no block of its own and changes GTE state.
static inline void _actorContactAccumulateAvoidanceStep(GfxCoord* coord, ActorContactSteerScratch* steerScratch, s16 parentYaw, s16 signedDistance, SVECTOR* pushDelta)
{
    // Scale the normalized parent-frame yaw axis into a signed step.
    gfxRotMatrixY(&steerScratch->rot, parentYaw, GRAPHICS_ROTATION_REPLACE);
    gfxReadMatrixZAxis(&steerScratch->rot, &steerScratch->dir);
    VectorNormalSS(&steerScratch->dir, &steerScratch->dir);
    gte_lddp(signedDistance);
    gte_ldsv(&steerScratch->dir);
    gte_gpf12();
    gte_stsv(&steerScratch->dir);

    // Accumulate the same horizontal step at each destination's integer width.
    pushDelta->vx     += steerScratch->dir.vx;
    pushDelta->vz     += steerScratch->dir.vz;
    coord->coord.t[0] += steerScratch->dir.vx;
    coord->coord.t[2] += steerScratch->dir.vz;
}

/// Applies short avoidance pushes away from player and enemy body contacts.
///
/// Reads at most `contactCount` records (0..255), stopping at the first zero
/// key or after eight eligible body contacts. Contact flags are not tested.
/// Returns 1 if a player or companion body was encountered during this scan,
/// even when its bearing is rejected and no push is applied; otherwise 0.
///
/// Each bearing within a quarter turn of every other collected bearing adds
/// a nominal ten-unit step away from that body to the coordinate's parent-space
/// XZ translation. `pushDelta` receives the summed signed-halfword correction,
/// with Y zero and pad untouched. When viewReady or actorsFrozen equals 1,
/// returns 0 without changing the coordinate or output.
///
/// `coord->workm` must already be composed in the same frame as the contact
/// points. Bearings use XZ, or XY when the normalized cached Y axis has a Z
/// magnitude of at least 2072 (4096 is unit length). The local yaw converts each
/// bearing to the parent frame; callers must mark changed coordinates dirty.
/// Borrows the inputs and an initialized scratch stack only for this call.
static ACTOR_CONTACT_STEER_RESULT _actorContactApplyAvoidancePushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, SVECTOR* pushDelta)
{
    enum {
        ACTOR_CONTACT_AVOIDANCE_NO_KEY                 = 0,     // First keyless record ends this scan
        ACTOR_CONTACT_AVOIDANCE_XY_AXIS_Z_THRESHOLD    = 0x818, // Normalized Y-axis Z magnitude; ONE (4096) is unit length
        ACTOR_CONTACT_AVOIDANCE_MAX_BEARING_SEPARATION = 0x400, // Quarter turn in 4096-unit angles
        ACTOR_CONTACT_AVOIDANCE_PUSH_DISTANCE          = 10     // Game-coordinate units per surviving bearing
    };
    ActorContactSteerScratch* scratch;
    s16                       pushYaw;

    if (gGameSession->viewReady == 1 || gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }

    scratch          = SCRATCH_STACK_RESERVE_BLOCK(ActorContactSteerScratch);
    scratch->blocked = 0;
    pushDelta->vz    = 0;
    pushDelta->vy    = 0;
    pushDelta->vx    = 0;

    // Choose a bearing plane from the cached basis and retain its heading.
    gfxReadMatrixYAxis(&coord->workm, &scratch->dir);
    VectorNormalSS(&scratch->dir, &scratch->dir);

    if (ABS(scratch->dir.vz) < ACTOR_CONTACT_AVOIDANCE_XY_AXIS_Z_THRESHOLD) {
        scratch->heading = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        scratch->heading = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    scratch->origin.vx = coord->workm.t[0];
    scratch->origin.vy = coord->workm.t[1];
    scratch->origin.vz = coord->workm.t[2];
    scratch->count     = 0;

    // Collect body bearings; the player-contact result is independent of pushing.
    for (scratch->i = 0; scratch->i < contactCount; scratch->i++) {
        if (contacts[scratch->i].key.value == ACTOR_CONTACT_AVOIDANCE_NO_KEY) {
            break;
        }
        scratch->kind = contacts[scratch->i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        switch (scratch->kind) {
            case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                scratch->blocked = 1;
                // Player bodies participate in the same avoidance as enemy bodies.
            case WORLD_COLLISION_CONTACT_ENEMY_BODY:
                break;
            default:
                continue;
        }

        if (ABS(scratch->dir.vz) < ACTOR_CONTACT_AVOIDANCE_XY_AXIS_Z_THRESHOLD) {
            scratch->bearing[scratch->count] = _actorAngleBearingXZ(&contacts[scratch->i].point, &scratch->origin);
        } else {
            scratch->bearing[scratch->count] = _actorAngleBearingXY(&contacts[scratch->i].point, &scratch->origin);
        }
        scratch->kept[scratch->count] = 1;
        scratch->count++;
        if (scratch->count >= ARRAY_SIZE(scratch->bearing)) {
            break;
        }
    }

    // Reject conflicting bearings, then push away in the coordinate's parent frame.
    for (scratch->i = 0; scratch->i < scratch->count; scratch->i++) {
        for (scratch->j = scratch->i + 1; scratch->j < scratch->count; scratch->j++) {
            scratch->diff = _actorAngleNormalizeYaw(scratch->bearing[scratch->i] - scratch->bearing[scratch->j]);
            if (abs(scratch->diff) > ACTOR_CONTACT_AVOIDANCE_MAX_BEARING_SEPARATION) {
                scratch->kept[scratch->i] = 0;
                scratch->kept[scratch->j] = 0;
            }
        }
        if (scratch->kept[scratch->i] != 0) {
            pushYaw = (scratch->bearing[scratch->i] - scratch->heading) +
                      ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            scratch->diff = pushYaw;
            _actorContactAccumulateAvoidanceStep(coord, scratch, pushYaw, -ACTOR_CONTACT_AVOIDANCE_PUSH_DISTANCE, pushDelta);
        }
    }

    // The release leaves the result byte intact until the next reservation.
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactSteerScratch);
    return scratch->blocked != 0;
}
