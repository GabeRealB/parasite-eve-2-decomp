/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Combat state 6: runs `_madChaserRecoilLight` or `_madChaserRecoilLightRecover`,
/// chosen by `subState`.
void madChaserRecoilLightState(Task* arg0)
{
    MadChaserWork* work                = (MadChaserWork*)arg0->work;
    void           (*states[2])(Task*) = {
        _madChaserRecoilLight,
        _madChaserRecoilLightRecover,
    };

    states[(s16)work->subState](arg0);
}
