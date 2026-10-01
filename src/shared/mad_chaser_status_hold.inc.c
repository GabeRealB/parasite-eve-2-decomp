/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, requests animation 0xB; once the enemy's
/// buildup countdown (`Gp_TickObjFlag2`) runs out, moves the state machine to state 3.
void madChaserStatusHold(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;

    if ((madChaserAnimEnded(arg0) << 0x10) != 0) {
        work            = (MadChaserWork*)arg0->work;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
        work2            = (MadChaserWork*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}
