/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Returns heavy recoil to walking or alert when slot 1 reaches a boundary.
///
/// Requires live work/enemy storage and the interrupted stance in stateScratch.
/// Boundary, jump and held-pose status all qualify without consuming the status.
/// Upright stance selects combat walking; low stance claims alert ownership and
/// selects alert behavior. Both reset the sub-state and retain task/animation.
static void _madChaserRecoilHeavyEnd(Task* task)
{
    MadChaserWork* work;

    work = task->work;
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
        } else {
            _madChaserSetAlertHold(task, 1);
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_ALERT);
        }
    }
}
