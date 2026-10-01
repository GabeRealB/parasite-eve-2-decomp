/* Part of the Odd Stranger library; see odd_stranger.h. */

/// The `0x7D3` handler of the `D_actor_401800_80155A80` table: maps the
/// requested state onto the work block's `field_89E` animation slot (5 selects
/// nothing), then resets the actor to state `0x11` with `field_2` cleared.
s32 oddStrangerPlayMessage(Task* arg0, s32 arg1, AnimationPlayRequest* arg2)
{
    OddStrangerRigWork* work = arg0->work;

    switch (arg2->animationId) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}
