/* Part of the actor contacts library; see actor_contacts.h. */

/// Applies one bearing-derived X/Z translation in the coordinate's parent frame.
///
/// The scratch direction is normalized at Q12, scaled by negative pushDistance
/// and narrowed by the GTE. Requires live scratch/coordinate and GTE state.
static inline void _actorContactApplyBearingStep(GfxCoord* coord, ActorContactBearingPushScratch* scratch, s16 pushDistance)
{
    gfxRotMatrixY(&scratch->rot,
                  scratch->bearing[scratch->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                  GRAPHICS_ROTATION_REPLACE);
    gfxReadMatrixZAxis(&scratch->rot, &scratch->forward);
    VectorNormalSS(&scratch->forward, &scratch->forward);
    gte_lddp(-pushDistance);
    gte_ldsv(&scratch->forward);
    gte_gpf12();
    gte_stsv(&scratch->delta);
    coord->coord.t[0] += scratch->delta.vx;
    coord->coord.t[2] += scratch->delta.vz;
}

/// Pushes a coordinate away from clustered player/companion and enemy body contacts.
///
/// Borrows a live coordinate chain and contact prefix; `contactCount` is 0..16
/// and a zero key terminates it. Bearings use the full-chain composition frame,
/// relative to a point 4096 units along local Z. Only bearings within a quarter
/// turn of one another qualify. The pair scan applies a step only on reaching
/// the terminator or a qualifying comparison at the final slot; self and skipped
/// slots do not trigger it. In particular, a one-record prefix never pushes.
/// Each applied step translates local X/Z by the negative normalized direction
/// times `pushDistance`, in parent-coordinate units.
/// Returns 1 if a step was applied, even for zero distance, otherwise 0.
/// `viewReady == 1` suppresses all work. Requires initialized scratch/GTE state;
/// scratch is released on return and inputs other than the coordinate are retained
/// only for the call. Angles use 4096 units per turn; points narrow to 16 bits.
static s32 _actorContactApplyBearingPushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, s16 pushDistance)
{
    enum {
        ACTOR_CONTACT_BEARING_FORWARD_DISTANCE = 4096,
        ACTOR_CONTACT_BEARING_MAX_SEPARATION   = ACTOR_TRANSFORM_ANGLE_TURN / 4
    };
    ActorContactBearingPushScratch* scratch;
    s32                             pushApplied;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    SCRATCH_STACK_RESERVE_BLOCK(ActorContactBearingPushScratch);
    scratch = SCRATCH_STACK_CURSOR(ActorContactBearingPushScratch);
    // Compare the origin and facing point in the contact records' composition frame.
    scratch->origin.vx = coord->coord.t[0];
    scratch->origin.vy = coord->coord.t[1];
    scratch->origin.vz = coord->coord.t[2];

    _actorContactTransformPointToChainRoot(coord->parent, &scratch->origin);

    scratch->forward.vx = 0;
    scratch->forward.vy = 0;
    scratch->forward.vz = ACTOR_CONTACT_BEARING_FORWARD_DISTANCE;

    _actorContactTransformStagedPointToChainRoot(coord, &scratch->forward);

    for (scratch->i = 0; scratch->i < contactCount; scratch->i++) {
        if (contacts[scratch->i].key.value == 0) {
            scratch->bearing[scratch->i] = ACTOR_CONTACT_BEARING_PUSH_END;
            break;
        }
        scratch->kind = contacts[scratch->i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if ((scratch->kind != WORLD_COLLISION_CONTACT_PLAYER_BODY) && (scratch->kind != WORLD_COLLISION_CONTACT_ENEMY_BODY)) {
            scratch->bearing[scratch->i] = ACTOR_CONTACT_BEARING_PUSH_SKIP;
        } else {
            scratch->delta.vx            = contacts[scratch->i].point.vx - scratch->origin.vx;
            scratch->delta.vy            = contacts[scratch->i].point.vy - scratch->origin.vy;
            scratch->delta.vz            = contacts[scratch->i].point.vz - scratch->origin.vz;
            scratch->bearing[scratch->i] = ratan2(scratch->delta.vx, scratch->delta.vz);

            scratch->delta.vx            = scratch->forward.vx - scratch->origin.vx;
            scratch->delta.vy            = scratch->forward.vy - scratch->origin.vy;
            scratch->delta.vz            = scratch->forward.vz - scratch->origin.vz;
            scratch->bearing[scratch->i] = scratch->bearing[scratch->i] - ratan2(scratch->delta.vx, scratch->delta.vz);

            scratch->bearing[scratch->i] = _actorAngleNormalizeYaw(scratch->bearing[scratch->i]);
        }
    }

    // Only a terminator or a successful final-slot comparison applies a step.
    scratch->pushed = 0;
    for (scratch->i = 0; scratch->i < contactCount; scratch->i++) {
        if (scratch->bearing[scratch->i] == ACTOR_CONTACT_BEARING_PUSH_END) {
            break;
        }
        if (scratch->bearing[scratch->i] == ACTOR_CONTACT_BEARING_PUSH_SKIP) {
            continue;
        }
        for (scratch->j = 0; scratch->j < contactCount; scratch->j++) {
            if (scratch->i == scratch->j) {
                continue;
            }
            if (scratch->bearing[scratch->j] == ACTOR_CONTACT_BEARING_PUSH_SKIP) {
                continue;
            }
            if (scratch->bearing[scratch->j] != ACTOR_CONTACT_BEARING_PUSH_END) {
                scratch->diff = scratch->bearing[scratch->j] - scratch->bearing[scratch->i];
                scratch->diff = _actorAngleNormalizeYaw(scratch->diff);
                if (abs(scratch->diff) > ACTOR_CONTACT_BEARING_MAX_SEPARATION) {
                    break;
                }
                if (scratch->bearing[scratch->j] != ACTOR_CONTACT_BEARING_PUSH_END) {
                    if (scratch->j + 1 < contactCount) {
                        continue;
                    }
                }
            }
            scratch->pushed = 1;
            _actorContactApplyBearingStep(coord, scratch, pushDistance);
            break;
        }
    }

    pushApplied = scratch->pushed;
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactBearingPushScratch);
    return pushApplied;
}
