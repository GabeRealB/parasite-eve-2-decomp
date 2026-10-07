/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Consumes a pending combat hit reaction when the hit latch is set.
///
/// Requires live Mad Chaser work in the combat task state. Reactions 1..5
/// select light recoil, heavy recoil, status hold, heavy recoil and knockdown,
/// restarting the selected behavior at sub-state zero. Other reactions only
/// clear the request. Returns 1 exactly when hitTaken equals 1, including an
/// absent or unsupported reaction, and 0 otherwise. The hit latch is retained.
static s16 _madChaserTakeHitRequest(Task* task)
{
    return _madChaserTakeHitReaction(task);
}
