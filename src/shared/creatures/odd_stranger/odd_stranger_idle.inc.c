/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Holds `ODD_STRANGER_STATE_DOWN` until its randomized rest time expires.
///
/// Seeds the signed tick countdown on entry from `downFramesBase` plus 0..15
/// (0..7 in actor_401800). The current clip chooses back or front recovery;
/// nonpositive HP overrides recovery with `ODD_STRANGER_STATE_DEATH_BURN`.
/// The actor_401800 build also advances the animation while down. Requires live work and enemy.
static void _oddStrangerDown(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        RANDOM_LCG_ADVANCE();
        work->stateTimer = work->downFramesBase + (RANDOM_LCG_DRAW() & ODD_STRANGER_DOWN_JITTER_MASK);
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
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401800
    _oddStrangerDriveAnimation(task);
#endif
}
