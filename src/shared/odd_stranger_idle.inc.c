/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Idle state: on entry the `stateTimer` countdown is seeded from `downFramesBase` plus
/// a 0-15 draw from `gRandomLcgState`. When it runs out, clips 0xB/0x17 move the
/// actor to state 0xF and clips 0xC/0x18/0x19 to state 0x10; a spent enemy HP
/// moves it to state 0x15 whatever else happened.
void oddStrangerIdle(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateTimer = work->downFramesBase + ((gRandomLcgState >> 16) & ODD_STRANGER_IDLE_JITTER_MASK);
    }
    if (--work->stateTimer < 0) {
        switch (work->animId) {
            case 0xB:
            case 0x17:
                work->state = ODD_STRANGER_STATE_RISE_BACK;
                break;
            case 0xC:
            case 0x18:
            case 0x19:
                work->state = ODD_STRANGER_STATE_RISE_FRONT;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->state = ODD_STRANGER_STATE_DEATH_BURN;
    }
#if ODD_STRANGER_VARIANT == 2
    oddStrangerDrive(arg0);
#endif
}
