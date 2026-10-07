/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Applies a normalized yaw-axis step to the walker and its accumulated XZ push.
///
/// `parentYaw` uses 4096 units per turn; `signedDistance` is in game-coordinate
/// units along that yaw's +Z axis; a negative distance reverses the step.
/// Borrows the caller's live scratch block, overwrites its rotation and direction,
/// and retains GTE Q12 scaling.
/// Adds XZ in full-width parent-coordinate translation, narrowing the summed
/// `push` to signed halfwords. Y stays unchanged. The caller initializes `push`
/// and dirties the coordinate after the step; no pointer is retained.
static inline void _bossStrangerAccumulateAvoidanceStep(BossStrangerWalker* walker, ActorContactSteerScratch* scratch, s16 parentYaw, s16 signedDistance)
{
    gfxRotMatrixY(&scratch->rot, parentYaw, GRAPHICS_ROTATION_REPLACE);
    gfxReadMatrixZAxis(&scratch->rot, &scratch->dir);
    VectorNormalSS(&scratch->dir, &scratch->dir);
    gte_lddp(signedDistance);
    gte_ldsv(&scratch->dir);
    gte_gpf12();
    gte_stsv(&scratch->dir);
    walker->push.vx           += scratch->dir.vx;
    walker->push.vz           += scratch->dir.vz;
    walker->coord->coord.t[0] += scratch->dir.vx;
    walker->coord->coord.t[2] += scratch->dir.vz;
}

/// Nudges the walker away from compatible contact bearings and records the correction.
///
/// Unless `actorsFrozen` equals 1, clears `push` and `blocked`, scans at most
/// `avoidCount` records (0..255), and stops at a zero key or eight accepted
/// contacts. Accepts player/companion and enemy body kinds, plus any nonzero key
/// with a zero low halfword. Seeing a player body sets `blocked` even if its
/// bearing is later rejected. Contact flags and distances are not tested.
///
/// Each surviving bearing adds a nominal ten-unit step away in parent-frame
/// XZ and accumulates it in the signed-halfword `push`; Y stays zero. The
/// pairwise pass rejects every bearing more than a quarter turn from another
/// collected bearing before that outer bearing's push is considered.
/// It chooses XY rather than XZ when the normalized cached Y axis has Z
/// magnitude at least 2072 (4096 is unit length).
///
/// Requires a live coordinate with `workm` composed in the contact points'
/// frame, that many readable contact entries, and initialized scratch storage.
/// No pointer is retained; the enclosing tick dirties the coordinate cache.
static void _bossStrangerAvoidContacts(BossStrangerWalker* walker)
{
    enum {
        BOSS_STRANGER_AVOID_XY_AXIS_Z_THRESHOLD    = 0x818, // Normalized Q12 Y-axis Z magnitude
        BOSS_STRANGER_AVOID_MAX_BEARING_SEPARATION = 0x400, // Quarter turn
        BOSS_STRANGER_AVOID_PUSH_DISTANCE          = 10     // Game-coordinate units
    };
    ActorContactSteerScratch* scratch;
    s16                       pushYaw;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return;
    }

    walker->blocked = 0;
    walker->push.vz = 0;
    walker->push.vy = 0;
    walker->push.vx = 0;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorContactSteerScratch);

    // Choose the bearing plane and cache its heading before collecting contacts.
    gfxReadMatrixYAxis(&walker->coord->workm, &scratch->dir);
    VectorNormalSS(&scratch->dir, &scratch->dir);

    if (ABS(scratch->dir.vz) < BOSS_STRANGER_AVOID_XY_AXIS_Z_THRESHOLD) {
        scratch->heading = ratan2(-walker->coord->workm.m[2][0], walker->coord->workm.m[2][2]);
    } else {
        scratch->heading = -ratan2(-walker->coord->workm.m[0][2], walker->coord->workm.m[1][2]);
    }

    scratch->origin.vx = walker->coord->workm.t[0];
    scratch->origin.vy = walker->coord->workm.t[1];
    scratch->origin.vz = walker->coord->workm.t[2];
    scratch->count     = 0;

    for (scratch->i = 0; scratch->i < walker->avoidCount; scratch->i++) {
        if (walker->avoidRecs[scratch->i].key.value == 0) {
            break;
        }
        scratch->kind = walker->avoidRecs[scratch->i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if (scratch->kind != WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            if (scratch->kind != WORLD_COLLISION_CONTACT_ENEMY_BODY && (u16)walker->avoidRecs[scratch->i].key.value != 0) {
                continue;
            }
        } else {
            walker->blocked = 1;
        }

        if (ABS(scratch->dir.vz) < BOSS_STRANGER_AVOID_XY_AXIS_Z_THRESHOLD) {
            scratch->bearing[scratch->count] =
                _actorAngleBearingXZ(&walker->avoidRecs[scratch->i].point, &scratch->origin);
        } else {
            scratch->bearing[scratch->count] =
                _actorAngleBearingXY(&walker->avoidRecs[scratch->i].point, &scratch->origin);
        }
        scratch->kept[scratch->count] = 1;
        scratch->count++;
        if (scratch->count >= ARRAY_SIZE(scratch->bearing)) {
            break;
        }
    }

    // Reject conflicting pairs while applying the surviving outer bearings.
    for (scratch->i = 0; scratch->i < scratch->count; scratch->i++) {
        for (scratch->j = scratch->i + 1; scratch->j < scratch->count; scratch->j++) {
            scratch->diff = _actorAngleNormalizeYaw(scratch->bearing[scratch->i] - scratch->bearing[scratch->j]);
            if (abs(scratch->diff) > BOSS_STRANGER_AVOID_MAX_BEARING_SEPARATION) {
                scratch->kept[scratch->i] = 0;
                scratch->kept[scratch->j] = 0;
            }
        }
        if (scratch->kept[scratch->i] != 0) {
            pushYaw = (scratch->bearing[scratch->i] - scratch->heading) +
                      ratan2(-walker->coord->coord.m[2][0], walker->coord->coord.m[2][2]);
            scratch->diff = pushYaw;
            _bossStrangerAccumulateAvoidanceStep(walker, scratch, pushYaw, -BOSS_STRANGER_AVOID_PUSH_DISTANCE);
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(ActorContactSteerScratch);
}
