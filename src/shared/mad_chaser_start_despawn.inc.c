/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Puts the task in state 5, the despawn phase, with `field_420` and
/// `field_422` cleared.
void madChaserStartDespawn(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}
