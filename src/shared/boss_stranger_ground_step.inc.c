/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Turns the 16.16 collision delta from worldCollisionResolvePushback into a whole-unit step
/// rounded away from zero, and adds a 0x10 fall unless `lockHeight` pins Y.
/// Y is applied in bands (+8 hop above 0x20, -0x20 drop below -0x20, otherwise
/// the plain step). `offOrigin` is then 1 when the local X or Z translation
/// is nonzero; nothing reads it.
void bossStrangerApplyGroundStep(BossStrangerWalker* work)
{
    BossStrangerGroundStepScratch* head;
    BossStrangerGroundStepScratch* s;
    s32                            valx;
    s32                            valy;
    s32                            valz;
    s32                            dx;
    s32                            dy;
    s32                            dz;
    s32                            y;

    head                                                = SCRATCH_STACK_CURSOR(BossStrangerGroundStepScratch);
    SCRATCH_STACK_CURSOR(BossStrangerGroundStepScratch) = head - 1;
    s                                                   = head - 1;
    if (worldCollisionResolvePushback(work->recs, &s->delta, work->recCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        dx         = head[-1].delta.fixed.vx.halves.integer;
        dz         = s->delta.fixed.vz.halves.integer;
        s->move.vx = dx;
        s->move.vz = dz;
        valx       = head[-1].delta.fixed.vx.word;
        if ((valx & 0xFFFF) != 0) {
            if (valx > 0) {
                s->move.vx++;
            } else {
                s->move.vx--;
            }
        }
        valz = s->delta.fixed.vz.word;
        if ((valz & 0xFFFF) != 0) {
            if (valz > 0) {
                s->move.vz++;
            } else {
                s->move.vz--;
            }
        }
        if (work->lockHeight == 0) {
            dy         = s->delta.fixed.vy.halves.integer;
            valy       = s->delta.fixed.vy.word;
            s->move.vy = s->move.vy + dy;
            if ((valy & 0xFFFF) != 0) {
                if (valy > 0) {
                    s->move.vy++;
                } else {
                    s->move.vy--;
                }
            }
        } else {
            s->move.vy = 0;
        }
    } else {
        s->move.vx = 0;
        s->move.vy = 0;
        s->move.vz = 0;
    }
    if (work->lockHeight == 0) {
        s->move.vy += 0x10;
    }
    work->moveDelta          = s->move;
    work->coord->coord.t[0] += s->move.vx;
    if (s->move.vy >= 0x21) {
        work->coord->coord.t[1] += 8;
    }
    if (s->move.vy < -0x20) {
        work->coord->coord.t[1] -= 0x20;
    }
    y = s->move.vy;
    if (ABS(y) < 0x20) {
        work->coord->coord.t[1] += y;
    }
    work->coord->coord.t[2] += s->move.vz;
    if (work->coord->coord.t[0] != 0 || work->coord->coord.t[2] != 0) {
        work->offOrigin = 1;
    } else {
        work->offOrigin = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerGroundStepScratch);
}
