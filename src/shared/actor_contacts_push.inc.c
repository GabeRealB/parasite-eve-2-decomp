/* Part of the actor contacts library; see actor_contacts.h. */

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings are taken in world space from the
/// frame's position, relative to the point one unit in front of it. Returns
/// whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
static s32 ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push)
{
    OverlayBisectorScratch* st;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    SCRATCH_STACK_RESERVE_BLOCK(OverlayBisectorScratch);
    st         = SCRATCH_STACK_CURSOR(OverlayBisectorScratch);
    st->eye.vx = (u16)coord->coord.t[0];
    st->eye.vy = (u16)coord->coord.t[1];
    st->eye.vz = (u16)coord->coord.t[2];

    overlayToWorld(coord->parent, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key.value == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key.value & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->angle[st->i] = ratan2(st->delta.vx, st->delta.vz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)st->aim.vz - (u16)st->eye.vz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, st->delta.vz);

            st->angle[st->i] = actorWrapAngle(st->angle[st->i]);
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                st->diff = actorWrapAngle(st->diff);
                if (abs(st->diff) > 0x400) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    hit = st->hit;
    SCRATCH_STACK_RELEASE_BLOCK(OverlayBisectorScratch);
    return hit;
}
