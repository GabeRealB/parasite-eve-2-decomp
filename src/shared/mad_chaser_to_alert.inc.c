/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Sets the state machine to state 5, the alert, with `field_422` cleared.
void madChaserToAlertState(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}
