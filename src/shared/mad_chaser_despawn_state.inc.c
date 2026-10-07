/* Part of the Mad Chaser library; see mad_chaser.h. */

// Bind MAD_CHASER_DESPAWN_STATE to a declared static void(Task*) handler before
// inclusion, then undefine it. It names only this definition; the callbacks are
// the Mad Chaser counter initializer and timed despawn handlers.

/// Runs task state 5: initializes the despawn counter, then waits for destruction.
///
/// Requires live Mad Chaser work with `state` in 0..1: 0 clears `stateFrames`
/// and advances to 1; 1 counts 36 subsequent ticks before destroying the enemy.
/// The task and enemy must remain live until destruction, after which neither
/// may be accessed. The signed-halfword selector is retained for dispatch.
static void MAD_CHASER_DESPAWN_STATE(Task* task)
{
    MadChaserWork* work               = task->work;
    TaskFunc       behaviorHandlers[] = {
        _madChaserAdvanceBehaviorState,
        _madChaserDespawn,
    };

    behaviorHandlers[(s16)work->state](task);
}
