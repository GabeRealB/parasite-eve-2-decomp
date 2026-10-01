/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Releases the gSceneCombatState reference and requests the settle animation that
/// follows the playing one (5 or 6 after animation 8), then ticks it and
/// advances.
void hopperDeathSettle(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* work3;
    Actor341700Work* work4;
    s16              anim;
    s16              next;

    work = (Actor341700Work*)arg0->work;
    Gp_ReleaseStateF0Add(arg0, 0);
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work2            = (Actor341700Work*)arg0->work;
            work2->field_426 = 4;
            work2->field_41C = 0x10;
            work2->field_418 = 5;
            work2->field_414 = 1;
        } else {
            work3            = (Actor341700Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 6;
            work3->field_414 = 1;
        }
    } else {
        next             = gHopperSettleAnims[anim - 1];
        work4            = (Actor341700Work*)arg0->work;
        work4->field_426 = 4;
        work4->field_41C = 0x10;
        work4->field_418 = next;
        work4->field_414 = 1;
    }
    hopperTickAnim(arg0);
    work->field_420++;
}
