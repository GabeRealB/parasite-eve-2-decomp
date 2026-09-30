/* Part of the hopping enemy library; see hopping_enemy.h. */

/// hopperDeathSettle without releasing the Gp_StateF0 reference: requests the
/// follow-up settle animation, ticks it and advances.
void hopperDeathSettleQuiet(Task* arg0)
{
    Actor341700Work* work;
    s16              anim;
    s16              next;

    work = (Actor341700Work*)arg0->work;
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 5;
            work->field_414 = 1;
        } else {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 6;
            work->field_414 = 1;
        }
    } else {
        next            = gHopperSettleAnims[anim - 1];
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = next;
        work->field_414 = 1;
    }
    hopperTickAnim(arg0);
    work->field_420++;
}
