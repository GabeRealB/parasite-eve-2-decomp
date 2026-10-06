/* Part of the Rat library; see rat.h. */

/// Reads the current yaw from the root rotation and steps it toward the wanted
/// heading by the turn rate, taking the short way round the 0x1000 circle and
/// snapping when within one step. Rebuilds the root rotation from that yaw
/// alone.
void ratTurn(Task* arg0)
{
    RatWork*          work;
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

    sc    = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->targetYaw;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->yaw = ang;
    if (adiff < 0x800) {
        step = work->turnRate;
        if (step >= adiff) {
            work->yaw = want;
        } else {
            next = work->yaw;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->yaw = next;
        }
    } else {
        step = work->turnRate;
        if (diff > 0 ? step >= 0x1000 - diff : step >= 0x1000 + diff) {
            work->yaw = work->targetYaw;
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
    sc->rot.vx = 0;
    sc->rot.vy = work->yaw;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
