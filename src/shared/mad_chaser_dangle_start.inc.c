/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Sets `field_432`, requests animation 7 and advances the sub-state.
void madChaserDangleStart(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work             = (Actor341700Work*)arg0->work;
    work->field_432  = 1;
    work2            = (Actor341700Work*)arg0->work;
    work2->field_41C = 0x10;
    work2->field_418 = 7;
    work2->field_414 = 2;
    work->field_422  = work->field_422 + 1;
}
