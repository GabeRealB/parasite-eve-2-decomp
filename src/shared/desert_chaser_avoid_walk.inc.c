/* Part of the Desert Chaser library; requires the armed DESERT_CHASER_BUILD. */

/// Builds and accumulates a signed horizontal push in the coordinate's parent frame.
///
/// The caller lends its scratch block and initialized displacement; XYZ vectors
/// use signed halfwords and translation uses words. Y and pad are untouched.
/// `pushYaw` uses 4096 units per turn and `stepDistance` uses parent-frame units.
/// Normalization and GTE scaling retain signed-halfword saturation; all storage
/// is borrowed and must not overlap the nested yaw workspace. GTE state changes.
/// The caller must invalidate composition after this translation change.
static inline void _desertChaserAccumulateAvoidanceStep(GfxCoord* coord, DesertChaserAvoidScratch* scratch, s16 pushYaw, s16 stepDistance, SVECTOR* displacement)
{
    gfxRotMatrixY(&scratch->rot, pushYaw, GRAPHICS_ROTATION_REPLACE);
    gfxReadMatrixZAxis(&scratch->rot, &scratch->dir);
    _actorMovementBuildDisplacement(&scratch->dir, stepDistance);
    displacement->vx  += scratch->dir.vx;
    displacement->vz  += scratch->dir.vz;
    coord->coord.t[0] += scratch->dir.vx;
    coord->coord.t[2] += scratch->dir.vz;
}

/// Applies short avoidance pushes and reports a catch-eligible player contact.
///
/// Reads at most `contactCount` records (0..255), stopping at the first zero
/// key or at the build's 8/16-bearing capacity. Player and enemy body contacts
/// supply bearings; companion body keys carry bit 0x80 and never raise the
/// reply. Returns 1 when an eligible player was scanned, even if its bearing
/// is rejected, otherwise 0. Contact flags are not tested.
///
/// Each bearing within a quarter turn of every other collected bearing adds
/// a nominal ten-unit step away from that body to parent-space XZ translation.
/// `displacement` receives the summed signed-halfword correction, with Y zero
/// and pad untouched. The caller composes the world matrix before entry and
/// marks it dirty afterwards. All inputs and output must be live and disjoint
/// from this call's scratch reservation and nested workspaces. The stack needs
/// 0x94 free bytes in the regular build or 0x7C in Water Tower, including the
/// nested yaw workspace. No pointers are retained; GTE state changes.
///
/// actorsFrozen/viewReady equal to 1 returns zero without touching the output.
/// The regular build has already reserved its 0x70-byte block on that path and
/// leaves it reserved; the Water Tower build reserves only after this test.
/// Other paths release the 0x70/0x58-byte block after the final push.
static s16 _desertChaserAvoidWalk(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, SVECTOR* displacement)
{
    enum {
        DESERT_CHASER_AVOID_COMPANION_KEY_BIT = 0x80,
        DESERT_CHASER_AVOID_PLANE_THRESHOLD   = 0x818,
        DESERT_CHASER_AVOID_QUARTER_TURN      = 0x400,
        DESERT_CHASER_AVOID_STEP_DISTANCE     = -10
    };
    DesertChaserAvoidScratch* scratch;
    s16                       pushYaw;

#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserAvoidScratch);
#endif

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }

#if DESERT_CHASER_BUILD == DESERT_CHASER_WATER_TOWER
    scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserAvoidScratch);
#endif

    scratch->blocked = 0;
    displacement->vz = 0;
    displacement->vy = 0;
    displacement->vx = 0;

    gfxReadMatrixYAxis(&coord->workm, &scratch->dir);
    VectorNormalSS(&scratch->dir, &scratch->dir);

    if (ABS(scratch->dir.vz) < DESERT_CHASER_AVOID_PLANE_THRESHOLD) {
        scratch->heading = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        scratch->heading = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    scratch->origin.vx = (u16)coord->workm.t[0];
    scratch->origin.vy = (u16)coord->workm.t[1];
    scratch->origin.vz = (u16)coord->workm.t[2];
    scratch->count     = 0;

    // Gather world bearings; companion contacts steer but cannot start a catch.
    for (scratch->i = 0; scratch->i < contactCount; scratch->i++) {
        if (contacts[scratch->i].key.value == 0) {
            break;
        }
        scratch->kind        = contacts[scratch->i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        scratch->nonBlocking = contacts[scratch->i].key.value & DESERT_CHASER_AVOID_COMPANION_KEY_BIT;
        switch (scratch->kind) {
            case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                if (scratch->nonBlocking == 0) {
                    scratch->blocked = 1;
                }
            case WORLD_COLLISION_CONTACT_ENEMY_BODY:
                break;
            default:
                continue;
        }

        if (ABS(scratch->dir.vz) < DESERT_CHASER_AVOID_PLANE_THRESHOLD) {
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

    // Reject opposed bearings, then convert each survivor to a parent-frame push.
    for (scratch->i = 0; scratch->i < scratch->count; scratch->i++) {
        for (scratch->j = scratch->i + 1; scratch->j < scratch->count; scratch->j++) {
            scratch->diff = _actorAngleNormalizeYaw(scratch->bearing[scratch->i] - scratch->bearing[scratch->j]);
            if (abs(scratch->diff) > DESERT_CHASER_AVOID_QUARTER_TURN) {
                scratch->kept[scratch->i] = 0;
                scratch->kept[scratch->j] = 0;
            }
        }
        if (scratch->kept[scratch->i] != 0) {
            pushYaw = ((u16)scratch->bearing[scratch->i] - (u16)scratch->heading) +
                      ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            scratch->diff = pushYaw;
            _desertChaserAccumulateAvoidanceStep(coord, scratch, pushYaw, DESERT_CHASER_AVOID_STEP_DISTANCE, displacement);
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserAvoidScratch);
    return scratch->blocked != 0;
}
