/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Dispatches the scripted vanish's detachment and timed teardown phases.
///
/// The hidden form's task state 7 requires live work/enemy/model storage and
/// work->state in 0..1: zero detaches and hides the enemy, one releases its
/// primitive buffer on frame 3 and destroys it from frame 36. Dispatch retains
/// the signed-halfword selector. The callback may begin teardown; no storage is
/// retained by this stack table. The separate despawn dispatcher uses task state 5.
static void _madChaserVanishState(Task* task)
{
    MadChaserWork* work               = task->work;
    TaskFunc       behaviorHandlers[] = {
        _madChaserVanish,
        _madChaserVanishFree,
    };

    behaviorHandlers[(s16)work->state](task);
}
