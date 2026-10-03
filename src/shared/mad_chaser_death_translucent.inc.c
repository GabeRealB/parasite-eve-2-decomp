/* Part of the Mad Chaser library; see mad_chaser.h. */

/// After 0x18 frames sets model flag 2, clears the frame counter, sets
/// `shadowHidden` and advances the state.
void madChaserDeathTurnTranslucent(Task* arg0)
{
    u16            ticks;
    MadChaserWork* work;
    TmdObject*     model;

    work              = (MadChaserWork*)arg0->work;
    model             = arg0->extra.tmd;
    ticks             = work->stateFrames + 1;
    work->stateFrames = ticks;
    if ((s16)ticks >= 0x18) {
        model->flags       = model->flags | TMD_OBJECT_SEMI_TRANS;
        work->stateFrames  = 0U;
        work->shadowHidden = 1;
        work->state        = work->state + 1;
    }
}
