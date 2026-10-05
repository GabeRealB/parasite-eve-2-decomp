/* Part of the actor contacts library; see actor_contacts.h. */

/// Steers `coord` away from the obstacles among the first `count` contact
/// records: collects the bearing of up to eight records of kind 0x10000 or
/// 0x30000 (in the XZ plane, or XY when the facing column is near vertical),
/// discards any pair more than 0x400 apart, and for each remaining bearing
/// nudges both `coord`'s translation and `*pos` a short step away from it. `*pos`
/// accumulates the total nudge. Returns whether any record was of kind
/// 0x10000; returns 0 at once when `gGameSession->viewReady` or `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen`
/// is 1.
static ACTOR_CONTACT_STEER_RESULT ActorContact_Steer(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos)
{
    ActorContactSteerScratch* s;
    s16                       diff;

    if (gGameSession->viewReady == 1 || gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }

    s          = SCRATCH_STACK_RESERVE_BLOCK(ActorContactSteerScratch);
    s->blocked = 0;
    pos->vz    = 0;
    pos->vy    = 0;
    pos->vx    = 0;

    gfxReadMatrixYAxis(&coord->workm, &s->dir);
    VectorNormalSS(&s->dir, &s->dir);

    if (ABS(s->dir.vz) < 0x818) {
        s->heading = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->heading = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->origin.vx = (u16)coord->workm.t[0];
    s->origin.vy = (u16)coord->workm.t[1];
    s->origin.vz = (u16)coord->workm.t[2];
    s->count     = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            break;
        }
        s->kind = recs[s->i].key.value & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->bearing[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->origin);
        } else {
            s->bearing[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->origin);
        }
        s->kept[s->count] = 1;
        s->count++;
        if (s->count >= ARRAY_SIZE(s->bearing)) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            s->diff = actorWrapAngle((u16)s->bearing[s->i] - (u16)s->bearing[s->j]);
            if (abs(s->diff) > 0x400) {
                s->kept[s->i] = 0;
                s->kept[s->j] = 0;
            }
        }
        if (s->kept[s->i] != 0) {
            diff = ((u16)s->bearing[s->i] - (u16)s->heading) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            gfxRotMatrixY(&s->rot, diff, 1);
            gfxReadMatrixZAxis(&s->rot, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(ActorContactSteerScratch);
    return s->blocked != 0;
}
