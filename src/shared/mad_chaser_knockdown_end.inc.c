/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Resumes combat walking or alert after the recovery animation's boundary status.
///
/// Saved stance 1 selects combat walk; all other values select combat alert.
/// Resets the behavior sub-state, retaining task state, animation and counters.
/// Requires initialized work and the stance saved by `_madChaserKnockdownStart`.
static void _madChaserKnockdownEnd(Task* task)
{
    enum { MAD_CHASER_KNOCKDOWN_END_UPRIGHT_STANCE = 1 };
    MadChaserWork* work = task->work;

    if (_madChaserAnimHasBoundaryStatus(task)) {
        if (work->stateScratch == MAD_CHASER_KNOCKDOWN_END_UPRIGHT_STANCE) {
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
        } else {
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_ALERT);
        }
    }
}
