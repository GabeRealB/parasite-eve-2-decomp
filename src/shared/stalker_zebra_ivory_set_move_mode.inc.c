/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Message handler: sets the movement mode from the message's second word
/// (0..3 become modes 1..4).
void stalkerZebraIvorySetMoveMode(Task* arg0, s32 arg1, u16* arg2)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    switch (arg2[1]) {
        case 0:
            work->moveMode = 1;
            break;
        case 1:
            work->moveMode = 2;
            break;
        case 2:
            work->moveMode = 3;
            break;
        case 3:
            work->moveMode = 4;
            break;
    }
}
