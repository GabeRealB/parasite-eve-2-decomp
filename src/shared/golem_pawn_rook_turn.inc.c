/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Steps `yaw` from the root coordinate's own heading toward the
/// target `targetYaw` by `turnRate` per frame: within half a turn it closes on
/// the target directly (or, on animation 3, turns the other way by the step),
/// past that it goes round the long way, snapping onto the target once the
/// step would overshoot. The result rebuilds the root coordinate's matrix.
void golemPawnRookTurnTowardTarget(Task* arg0)
{
    GolemPawnRookWork* work;
    GfxCoord*          coord;
    SVECTOR*           rot;
    s32                ang;
    u16                want;
    s16                diff;
    s32                adiff;
    s32                step;
    s32                ustep;
    s32                wstep;
    s32                cur;
    s32                next;
    s32                wrapStep;

    rot   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->targetYaw;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->yaw = ang;
    if (adiff < 0x800) {
        step  = work->turnRate;
        ustep = (u16)work->turnRate;
        if (step >= adiff) {
            work->yaw = want;
        } else {
            if (work->anim == 3) {
                next = ang - ustep;
            } else {
                next = work->yaw;
                if (diff <= 0) {
                    next -= step;
                } else {
                    next += step;
                }
            }
            work->yaw = next;
        }
    } else {
        wstep = work->turnRate;
        if (diff > 0) {
            if (wstep >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (wstep >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->yaw = work->targetYaw;
        goto done;
    turn:
        if (work->anim == 3) {
            work->yaw = (u16)work->yaw - (u16)work->turnRate;
        } else {
            wrapStep = work->turnRate;
            cur      = work->yaw;
            if (diff > 0) {
                work->yaw = cur - wrapStep;
            } else {
                work->yaw = cur + wrapStep;
            }
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->yaw;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(8);
}
