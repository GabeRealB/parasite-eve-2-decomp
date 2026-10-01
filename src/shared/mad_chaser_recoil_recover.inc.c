/* Part of the Mad Chaser library; see mad_chaser.h. */

/// State handler: with `field_44F` 1, a pending request 1 while `field_41E`
/// is set queues animation 0xB (kind 2, speed 0x20); otherwise a consumed
/// request wins, and a hit moves to state 3. With `field_44F` clear, a hit
/// calls `madChaserSetAlertHold` and moves to state 5. The request test
/// compares against the constant 1, which CSE folds into the `field_44F`
/// register; writing `== work->field_44F` reloads the byte instead.
void madChaserRecoilRecover(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (work->field_44F == 1) {
        if (work->field_41E != 0 && work->field_448 == 1) {
            work->field_41C = 0x20;
            work->field_418 = 0xB;
            work->field_414 = 2;
            return;
        }
        if (madChaserTakeRequest(arg0) == 0 && madChaserIsHit(arg0)) {
            madChaserSetState(arg0, 3);
        }
    } else if (madChaserIsHit(arg0)) {
        madChaserSetAlertHold(arg0, 1);
        madChaserSetState(arg0, 5);
    }
}
