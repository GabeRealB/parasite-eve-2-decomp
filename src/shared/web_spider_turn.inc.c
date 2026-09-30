/* Part of the web spider library; see web_spider.h. */

/// Turn step, run every frame while `field_3A6` is non-zero: reads the
/// heading back from the coordinate, turns it toward `field_3A4` by at most
/// `field_3A6` the shorter way round the circle, keeps the result in
/// `field_3A2` and rebuilds the coordinate's rotation as that pure yaw.
void spiderTurnStep(Task* arg0)
{
    Actor105500Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_3A4;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_3A2 = ang;
    if (adiff < 0x800) {
        step = work->field_3A6;
        if (step >= adiff) {
            work->field_3A2 = want;
        } else {
            next = work->field_3A2;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_3A2 = next;
        }
    } else {
        step = work->field_3A6;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_3A2 = work->field_3A4;
        goto done;
    turn:
        wrapStep = work->field_3A6;
        cur      = work->field_3A2;
        if (diff > 0) {
            work->field_3A2 = cur - wrapStep;
        } else {
            work->field_3A2 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_3A2;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
