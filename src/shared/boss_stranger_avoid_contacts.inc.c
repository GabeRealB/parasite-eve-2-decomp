/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Unless actors are frozen, gathers the bearings of up to eight avoid-contact
/// records (kind 0x10000 also sets blocked). Agreeing bearings each push the
/// walker 10 units away along that direction, and the push is accumulated in
/// push.
void bossStrangerAvoidContacts(BossStrangerWalker* work)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return;
    }

    work->blocked = 0;
    work->push.vz = 0;
    work->push.vy = 0;
    work->push.vx = 0;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(OverlayAvoidScratch);
    s                        = SCRATCH_STACK_CURSOR(OverlayAvoidScratch);

    Gfx_MatrixCol1(&work->coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-work->coord->workm.m[2][0], work->coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-work->coord->workm.m[0][2], work->coord->workm.m[1][2]);
    }

    s->eye.vx = (u16)work->coord->workm.t[0];
    s->eye.vy = (u16)work->coord->workm.t[1];
    s->eye.vz = (u16)work->coord->workm.t[2];
    s->count  = 0;

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
            s->angle[s->count] =
                overlayBearingXZ((SVECTOR3*)&work->avoidRecs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] =
                overlayBearingXY((SVECTOR3*)&work->avoidRecs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            s->diff = overlayWrapAngle((u16)s->angle[s->i] - (u16)s->angle[s->j]);
            if (abs(s->diff) > 0x400) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-work->coord->coord.m[2][0], work->coord->coord.m[2][2]);
            s->diff = diff;
            gfxRotMatrixY(&s->m, diff, 1);
            gfxReadMatrixZAxis(&s->m, &s->dir);
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

    SCRATCH_STACK_CURSOR(u8) =
        SCRATCH_STACK_CURSOR(u8) + sizeof(OverlayAvoidScratch);
}
