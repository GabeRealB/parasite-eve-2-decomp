/* Part of the actor contacts library; see actor_contacts.h. */

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings use the contacts' composition frame,
/// measured from the frame's position relative to the point one unit in front
/// of it. Returns whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
static s32 ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push)
{
    ActorContactBearingPushScratch* st;
    s32                             pushed;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    SCRATCH_STACK_RESERVE_BLOCK(ActorContactBearingPushScratch);
    st            = SCRATCH_STACK_CURSOR(ActorContactBearingPushScratch);
    st->origin.vx = (u16)coord->coord.t[0];
    st->origin.vy = (u16)coord->coord.t[1];
    st->origin.vz = (u16)coord->coord.t[2];

    _actorContactTransformPointToChainRoot(coord->parent, &st->origin);

    st->forward.vx = 0;
    st->forward.vy = 0;
    st->forward.vz = 0x1000;

    _actorContactTransformStagedPointToChainRoot(coord, &st->forward);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key.value == 0) {
            st->bearing[st->i] = ACTOR_CONTACT_BEARING_PUSH_END;
            break;
        }
        st->kind = recs[st->i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->bearing[st->i] = ACTOR_CONTACT_BEARING_PUSH_SKIP;
        } else {
            st->delta.vx       = (u16)recs[st->i].point.vx - (u16)st->origin.vx;
            st->delta.vy       = (u16)recs[st->i].point.vy - (u16)st->origin.vy;
            st->delta.vz       = (u16)recs[st->i].point.vz - (u16)st->origin.vz;
            st->bearing[st->i] = ratan2(st->delta.vx, st->delta.vz);

            st->delta.vx       = (u16)st->forward.vx - (u16)st->origin.vx;
            st->delta.vy       = (u16)st->forward.vy - (u16)st->origin.vy;
            st->delta.vz       = (u16)st->forward.vz - (u16)st->origin.vz;
            st->bearing[st->i] = st->bearing[st->i] - ratan2(st->delta.vx, st->delta.vz);

            st->bearing[st->i] = _actorAngleNormalizeYaw(st->bearing[st->i]);
        }
    }

    st->pushed = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->bearing[st->i] == ACTOR_CONTACT_BEARING_PUSH_END) {
            break;
        }
        if (st->bearing[st->i] == ACTOR_CONTACT_BEARING_PUSH_SKIP) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->bearing[st->j] == ACTOR_CONTACT_BEARING_PUSH_SKIP) {
                continue;
            }
            if (st->bearing[st->j] != ACTOR_CONTACT_BEARING_PUSH_END) {
                st->diff = st->bearing[st->j] - st->bearing[st->i];
                st->diff = _actorAngleNormalizeYaw(st->diff);
                if (abs(st->diff) > 0x400) {
                    break;
                }
                if (st->bearing[st->j] != ACTOR_CONTACT_BEARING_PUSH_END) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->pushed = 1;
            gfxRotMatrixY(&st->rot,
                          st->bearing[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                          1);
            gfxReadMatrixZAxis(&st->rot, &st->forward);
            VectorNormalSS(&st->forward, &st->forward);
            gte_lddp(-push);
            gte_ldsv(&st->forward);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    pushed = st->pushed;
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactBearingPushScratch);
    return pushed;
}
