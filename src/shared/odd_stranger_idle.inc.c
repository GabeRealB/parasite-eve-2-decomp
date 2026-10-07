/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Holds `ODD_STRANGER_STATE_DOWN` until its randomized rest time expires.
///
/// Seeds the signed tick countdown on entry from `downFramesBase` plus 0..15
/// (0..7 in variant 2). The current clip chooses back or front recovery;
/// nonpositive HP overrides recovery with `ODD_STRANGER_STATE_DEATH_BURN`.
/// Variant 2 also advances the animation while down. Requires live work and enemy.
static void _oddStrangerDown(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateTimer = work->downFramesBase + ((gRandomLcgState >> 16) & ODD_STRANGER_IDLE_JITTER_MASK);
    }
    if (--work->stateTimer < 0) {
        switch (work->animId) {
            case ODD_STRANGER_ANIM_DOWN_BACK:
            case ODD_STRANGER_ANIM_STATUS_BACK:
                work->state = ODD_STRANGER_STATE_RISE_BACK;
                break;
            case ODD_STRANGER_ANIM_DOWN_FRONT:
            case ODD_STRANGER_ANIM_STATUS_FRONT:
            case ODD_STRANGER_ANIM_REFALL_FRONT:
                work->state = ODD_STRANGER_STATE_RISE_FRONT;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->state = ODD_STRANGER_STATE_DEATH_BURN;
    }
#if ODD_STRANGER_VARIANT == 2
    _oddStrangerDriveAnimation(task);
#endif
}
