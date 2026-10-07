/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Returns the lurk rise to idle when slot 1 reaches an animation boundary.
///
/// Requires initialized animation/work storage. A boundary, control jump or
/// held pose selects lurk idle at sub-state zero; other statuses leave the
/// behavior intact. Does not consume status or tick the animation.
static void _madChaserLurkRiseEnd(Task* task)
{
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        _madChaserSetBehaviorState(task, MAD_CHASER_LURK_STATE_IDLE);
    }
}
