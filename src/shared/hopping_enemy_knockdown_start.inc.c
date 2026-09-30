/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Knockdown: reads the stance of the playing animation and requests fall
/// animation 6 (upright) or 5, then advances.
void hopperKnockdownStart(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_44F = gHopperAnimStance[work->field_418 - 1];
    if (work->field_44F == 1) {
        Actor341700Work* w = (Actor341700Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        Actor341700Work* w = (Actor341700Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}
