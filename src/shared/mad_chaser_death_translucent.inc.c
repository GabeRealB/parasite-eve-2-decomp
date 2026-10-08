/* Part of the Mad Chaser library; see mad_chaser.h. */

#ifdef MAD_CHASER_SHRINK_DEATH_TRANSLUCENT_HANDLER
/// Selects translucent drawing without limb shadows for the scripted death shrink.
///
/// Requires the same live enemy's work block and model in shrink-death behavior 4,
/// after the caller has counted 24 frames with actors running. Resets
/// `stateFrames` to count the subsequent shrink frames and advances behavior to
/// 5, whose handler shrinks the saved root transform. Both pointers are borrowed;
/// the task owns their storage throughout the shrink.
static __inline__ void _madChaserShrinkDeathEnterTranslucentPhase(MadChaserWork* work, TmdObject* model)
{
    model->flags      |= TMD_OBJECT_SEMI_TRANS;
    work->stateFrames  = 0U;
    work->shadowHidden = 1;
    work->state++;
}

/// Waits 24 updating death frames before making the shrinking body translucent.
///
/// Requires live task-owned Mad Chaser work and a model in scripted shrink-death
/// behavior 4, with stateFrames reset on entry. The counter wraps as u16 and is
/// compared as s16; the death dispatcher calls this only while actors run.
/// At the threshold, enables semi-transparent model drawing, suppresses limb
/// shadows, resets the counter and advances the u16 behavior to 5. Animation,
/// model and work storage remain live for the subsequent shrink frames.
/// MAD_CHASER_SHRINK_DEATH_TRANSLUCENT_HANDLER selects the carrier-declared
/// static void(Task*) instance; an unbound inclusion defines ordinary death.
static void MAD_CHASER_SHRINK_DEATH_TRANSLUCENT_HANDLER(Task* task)
#else
/// After 0x18 frames sets model flag 2, clears the frame counter, sets
/// `shadowHidden` and advances the state.
void madChaserDeathTurnTranslucent(Task* task)
#endif
{
    enum { MAD_CHASER_DEATH_TRANSLUCENT_DELAY_FRAMES = 24 };
    u16            elapsedFrames;
    MadChaserWork* work;
    TmdObject*     model;

    work              = task->work;
    model             = task->extra.tmd;
    elapsedFrames     = work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames >= MAD_CHASER_DEATH_TRANSLUCENT_DELAY_FRAMES) {
#ifdef MAD_CHASER_SHRINK_DEATH_TRANSLUCENT_HANDLER
        _madChaserShrinkDeathEnterTranslucentPhase(work, model);
#else
        model->flags       = model->flags | TMD_OBJECT_SEMI_TRANS;
        work->stateFrames  = 0U;
        work->shadowHidden = 1;
        work->state        = work->state + 1;
#endif
    }
}
