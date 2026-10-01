/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Clears the frame counter `field_412` and advances `field_420`.
void madChaserAdvanceState(Task* arg0)
{
    MadChaserWork* work;

    work            = (MadChaserWork*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}
