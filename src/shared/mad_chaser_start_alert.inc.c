/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests animation 0xF, advances the sub-state and arms `gSceneCombatState`.
void madChaserStartAlert(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
    Gp_ArmStateF0(1);
}
