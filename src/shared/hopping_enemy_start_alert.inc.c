/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Requests animation 0xF, advances the sub-state and arms `Gp_StateF0`.
void hopperStartAlert(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
    Gp_ArmStateF0(1);
}
