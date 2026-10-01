/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Combat state 6: runs `madChaserRecoilLight` or `madChaserRecoilRecover`,
/// chosen by `field_422`.
void madChaserRecoilLightState(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        madChaserRecoilLight,
        madChaserRecoilRecover,
    };

    states[(s16)work->field_422](arg0);
}
