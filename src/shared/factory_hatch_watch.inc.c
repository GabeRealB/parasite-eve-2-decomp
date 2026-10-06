/* Part of the factory lift library; see factory_lift.h. */

/// State 0 of the cutscene sequence: edge-detects game flag 0x4E and re-arms
/// the sequence when it changes. A nibble of 1 after a 0 moves the sequence to
/// state 1, and a nibble of 0 after a 1 moves it to state 2; either transition
/// restarts `step`. Every call records the nibble in `prevFlag`.
s32 factoryHatchWatch(Task* task)
{
    FactoryHatchWork* work = task->work;
    u8                flag = gameFlagGetNibble(GAME_FLAG_FACTORY_HATCH_OPEN);

    if (flag == 1 && work->prevFlag == 0) {
        work->state = FACTORY_HATCH_STATE_OPEN;
        work->step  = 0;
    } else if (flag == 0 && work->prevFlag == 1) {
        work->state = FACTORY_HATCH_STATE_CLOSE;
        work->step  = 0;
    }
    work->prevFlag = flag;
    return 0;
}
