/* Part of the Mad Chaser library; see mad_chaser.h. */

/// State handler: with `stateScratch` 1, a pending request 1 while `hitTaken`
/// is set queues animation 0xB (kind 2, speed 0x20); otherwise a consumed
/// request wins, and a hit moves to state 3. With `stateScratch` clear, a hit
/// calls `_madChaserSetAlertHold` and moves to state 5. The request test
/// compares against the constant 1, which CSE folds into the `stateScratch`
/// register; writing `== work->stateScratch` reloads the byte instead.
void madChaserRecoilRecover(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (work->stateScratch == 1) {
        if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_LIGHT) {
            work->animRate    = 0x20;
            work->animId      = 0xB;
            work->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
            return;
        }
        if (_madChaserTakeHitReaction(arg0) == 0 && madChaserIsHit(arg0)) {
            _madChaserSetBehaviorState(arg0, MAD_CHASER_COMBAT_STATE_WALK);
        }
    } else if (madChaserIsHit(arg0)) {
        _madChaserSetAlertHold(arg0, 1);
        _madChaserSetBehaviorState(arg0, MAD_CHASER_COMBAT_STATE_ALERT);
    }
}
