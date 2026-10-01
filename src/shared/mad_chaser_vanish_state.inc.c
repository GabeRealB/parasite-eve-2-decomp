/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Task state 7: runs `madChaserVanish` or `madChaserVanishFree`, chosen by
/// `field_420`.
void madChaserVanishState(Task* arg0)
{
    MadChaserWork* work                = (MadChaserWork*)arg0->work;
    void           (*states[2])(Task*) = {
        madChaserVanish,
        madChaserVanishFree,
    };

    states[(s16)work->field_420](arg0);
}
