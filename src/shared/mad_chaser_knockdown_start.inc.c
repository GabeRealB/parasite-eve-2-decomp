/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Knockdown: reads the stance of the playing animation and requests fall
/// animation 6 (upright) or 5, then advances.
void madChaserKnockdownStart(Task* arg0)
{
    MadChaserWork* work;

    work            = (MadChaserWork*)arg0->work;
    work->field_44F = gMadChaserAnimStance[work->field_418 - 1];
    if (work->field_44F == 1) {
        MadChaserWork* w = (MadChaserWork*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        MadChaserWork* w = (MadChaserWork*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}
