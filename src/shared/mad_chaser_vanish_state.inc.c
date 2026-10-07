/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Task state 7: runs `_madChaserVanish` or `_madChaserVanishFree`, chosen by
/// `state`.
void madChaserVanishState(Task* arg0)
{
    MadChaserWork* work                = (MadChaserWork*)arg0->work;
    void           (*states[2])(Task*) = {
        _madChaserVanish,
        _madChaserVanishFree,
    };

    states[(s16)work->state](arg0);
}
