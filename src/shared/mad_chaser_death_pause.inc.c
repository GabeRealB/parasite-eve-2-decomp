/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Advances the state after two frames.
void madChaserDeathPause(Task* arg0)
{
    u16            ticks;
    MadChaserWork* work;

    work            = (MadChaserWork*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}
