/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Unless actors are frozen, gathers the bearings of up to eight avoid-contact
/// records (kind 0x10000 also sets blocked). Agreeing bearings each push the
/// walker 10 units away along that direction, and the push is accumulated in
/// push.
void bossStrangerAvoidContacts(BossStrangerWalker* work)
{
    ActorContactSteerScratch* s;
    s16                       diff;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return;
    }

    work->blocked = 0;
    work->push.vz = 0;
    work->push.vy = 0;
    work->push.vx = 0;

    s = SCRATCH_STACK_RESERVE_BLOCK(ActorContactSteerScratch);

    gfxReadMatrixYAxis(&work->coord->workm, &s->dir);
    VectorNormalSS(&s->dir, &s->dir);

    if (ABS(s->dir.vz) < 0x818) {
        s->heading = ratan2(-work->coord->workm.m[2][0], work->coord->workm.m[2][2]);
    } else {
        s->heading = -ratan2(-work->coord->workm.m[0][2], work->coord->workm.m[1][2]);
    }

    s->origin.vx = (u16)work->coord->workm.t[0];
    s->origin.vy = (u16)work->coord->workm.t[1];
    s->origin.vz = (u16)work->coord->workm.t[2];
    s->count     = 0;

    for (s->i = 0; s->i < work->avoidCount; s->i++) {
        if (work->avoidRecs[s->i].key.value == 0) {
            break;
        }
        s->kind = work->avoidRecs[s->i].key.value & 0xFFFF0000;
        if (s->kind != 0x10000) {
            if (s->kind != 0x30000 && (u16)work->avoidRecs[s->i].key.value != 0) {
                continue;
            }
        } else {
            work->blocked = 1;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->bearing[s->count] =
                _actorAngleBearingXZ(&work->avoidRecs[s->i].point, &s->origin);
        } else {
            s->bearing[s->count] =
                _actorAngleBearingXY(&work->avoidRecs[s->i].point, &s->origin);
        }
        s->kept[s->count] = 1;
        s->count++;
        if (s->count >= ARRAY_SIZE(s->bearing)) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            s->diff = _actorAngleNormalizeYaw(s->bearing[s->i] - s->bearing[s->j]);
            if (abs(s->diff) > 0x400) {
                s->kept[s->i] = 0;
                s->kept[s->j] = 0;
            }
        }
        if (s->kept[s->i] != 0) {
            diff = ((u16)s->bearing[s->i] - (u16)s->heading) +
                   ratan2(-work->coord->coord.m[2][0], work->coord->coord.m[2][2]);
            s->diff = diff;
            gfxRotMatrixY(&s->rot, diff, 1);
            gfxReadMatrixZAxis(&s->rot, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            work->push.vx           += s->dir.vx;
            work->push.vz           += s->dir.vz;
            work->coord->coord.t[0] += s->dir.vx;
            work->coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(ActorContactSteerScratch);
}
