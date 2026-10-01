/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests animation 0xF and advances the sub-state.
void madChaserLurkRiseStart(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}
