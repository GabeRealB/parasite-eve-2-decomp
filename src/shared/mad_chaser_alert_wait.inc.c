/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Advances the sub-state once the frame counter has passed 0x50.
void madChaserAlertWait(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}
