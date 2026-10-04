/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Turns the first enemy toward the player by at most 0x20 a frame: the heading
/// `heading` takes the XZ direction to the player outright when within 0x20,
/// and otherwise steps 0x20 the short way round the 0x1000 circle. The root's
/// rotation is then rebuilt from that heading alone, in 0x18 bytes of the
/// scratch stack.
void sucklercephTurnToPlayer(Task* arg0)
{
    SucklercephWork*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s16               cur;
    s32               want;
    s16               diff;
    s32               adiff;
    s16               turn;
    s16               wrap;
    s32               current;

    coord        = arg0->extra.tmd->coords;
    work         = arg0->work;
    sc           = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    sc->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = 0;
    sc->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    want         = ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF;
    cur          = work->heading & 0xFFF;
    diff         = want - cur;
    adiff        = diff >= 0 ? diff : -diff;
    turn         = diff;
    if (adiff < 0x21) {
        work->heading = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = work->heading;
        if (turn <= 0) {
            cur = current - 0x20;
        } else {
            cur = current + 0x20;
        }
        work->heading = cur;
    }
    sc->rot.vx = 0;
    sc->rot.vy = work->heading;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
