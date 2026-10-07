/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once slot 1 reports a boundary, jump or hold, requests animation 0xB; once the enemy's
/// buildup countdown (`damageTickEnemyBuildup`) runs out, moves the state machine to state 3.
void madChaserStatusHold(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;

    if ((_madChaserAnimHasBoundaryStatus(arg0) << 0x10) != 0) {
        work                  = (MadChaserWork*)arg0->work;
        work->animBlendFrames = 4;
        work->animRate        = ANIMATION_RATE_ONE;
        work->animId          = 0xB;
        work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    if (damageTickEnemyBuildup(arg0->spawnArg2.pointer) != 0) {
        work2           = (MadChaserWork*)arg0->work;
        work2->state    = 3;
        work2->subState = 0;
    }
}
