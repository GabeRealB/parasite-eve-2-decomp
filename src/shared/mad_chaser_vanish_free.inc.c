/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Releases the hidden model's primitive buffer, then destroys the vanished enemy.
///
/// Requires the detached task left by `_madChaserVanish`. Increments its u16
/// frame counter, testing it as s16: frame 3 frees the primitive buffer and
/// disables automatic buffer allocation; frame 36 or later destroys the enemy
/// and begins task teardown. No task or enemy access is valid after destruction.
static void _madChaserVanishFree(Task* task)
{
    enum {
        MAD_CHASER_VANISH_RELEASE_BUFFER_FRAME = 3,
        MAD_CHASER_VANISH_DESTROY_FRAME        = 36,
    };
    MadChaserWork* work;
    TmdObject*     model;
    u16            elapsedFrames;

    work              = task->work;
    model             = task->extra.tmd;
    elapsedFrames     = work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == MAD_CHASER_VANISH_RELEASE_BUFFER_FRAME) {
        tmdFreePrimitiveBuffer(model);
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    if ((s16)work->stateFrames >= MAD_CHASER_VANISH_DESTROY_FRAME) {
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}
