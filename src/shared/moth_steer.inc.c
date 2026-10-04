/* Part of the Moth library; see moth.h. */

/// Before the alert the yaw takes a random step of up to +-31 a frame; once
/// alerted it turns toward the player by 0x10 a frame, snapping when within
/// 0x10. The pitch takes a random step of up to +-63 clamped to +-0x100, and
/// the root rotation is rebuilt from pitch and yaw.
void mothSteer(Task* arg0)
{
    MothWork*         work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               random;
    s32               amount;
    s32               cur;
    s32               cur2;
    s32               cur3;
    s32               random2;
    s32               amount2;
    u16               want;
    s16               diff;
    s32               adiff;
    s16               turn;
    s16               wrap;

    sc    = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_2E6) {
        case 0:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            random          = gRandomLcgState >> 16;
            amount          = random & 0x1F;
            cur             = work->field_2DC;
            work->field_2DC = !(random & 0x20) ? cur - amount : cur + amount;
            break;
        case 1:
            sc->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vy = 0;
            sc->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            want         = ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF;
            diff         = want - (work->field_2DC & 0xFFF);
            adiff        = diff >= 0 ? diff : -diff;
            turn         = diff;
            if (adiff < 0x11) {
                work->field_2DC = want;
            } else {
                if (adiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0)
                        wrap = 0x1000 - diff;
                    turn = wrap;
                }
                cur2 = work->field_2DC;
                if (turn > 0) {
                    work->field_2DC = cur2 + 0x10;
                } else {
                    work->field_2DC = cur2 - 0x10;
                }
            }
            break;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    random2         = gRandomLcgState >> 16;
    amount2         = random2 & 0x3F;
    cur3            = work->field_2DA;
    work->field_2DA = !(random2 & 0x40) ? cur3 - amount2 : cur3 + amount2;
    if (work->field_2DA > 0x100) {
        work->field_2DA = 0x100;
    } else if (work->field_2DA < -0x100) {
        work->field_2DA = -0x100;
    }
    sc->rot.vx = work->field_2DA;
    sc->rot.vy = work->field_2DC;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
