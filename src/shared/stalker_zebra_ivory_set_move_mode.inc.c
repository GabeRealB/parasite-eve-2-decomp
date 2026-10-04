/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Message handler: records the room command in `roomCommand` from the
/// message's second word (0..3 become kinds 1..4).
void stalkerZebraIvorySetMoveMode(Task* arg0, s32 arg1, u16* arg2, s32 arg3)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    switch (arg2[1]) {
        case 0:
            work->roomCommand = 1;
            break;
        case 1:
            work->roomCommand = 2;
            break;
        case 2:
            work->roomCommand = 3;
            break;
        case 3:
            work->roomCommand = 4;
            break;
    }
}
