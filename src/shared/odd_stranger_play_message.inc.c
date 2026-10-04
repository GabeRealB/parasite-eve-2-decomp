/* Part of the Odd Stranger library; see odd_stranger.h. */

/// The `0x7D3` handler of the `D_actor_401800_80155A80` table: maps the
/// requested state onto the work block's `animId` animation slot (5 selects
/// nothing), then resets the actor to state `0x11` with `prevState` cleared.
s32 oddStrangerPlayMessage(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3)
{
    OddStrangerWork* work = arg0->work;

    switch (arg2->animationId) {
        case 0:
            work->animId = 0x22;
            break;
        case 1:
            work->animId = 0x23;
            break;
        case 2:
            work->animId = 0x24;
            break;
        case 3:
            work->animId = 0x25;
            break;
        case 4:
            work->animId = 0x27;
            break;
    }
    work->state     = ODD_STRANGER_STATE_DOWN;
    work->prevState = -1;
    return 0;
}
