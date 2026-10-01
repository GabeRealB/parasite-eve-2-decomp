/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Turns the pending action into a state change - 1 to state 3, 2 and 4 to
/// state 4, 3 to state 5 and 5 to state 0xF, or on the ceiling 3 and 5 to
/// state 0xE - and clears it. Returns 1 when it changed state.
s32 stalkerZebraIvoryTakePending(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    StalkerZebraIvoryWork* work2;

    work = (StalkerZebraIvoryWork*)arg0->work;
    if (work->onCeiling == 0) {
        switch (work->pendingAction) {
            case 1:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 3;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
            case 2:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
            case 3:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 5;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
            case 4:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
            case 5:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 0xF;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
        }
        work->pendingAction = 0;
        return 0;
    } else {
        switch (work->pendingAction) {
            case 1:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 3;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
            case 2:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
            case 3:
                work2           = (StalkerZebraIvoryWork*)arg0->work;
                work2->state    = 0xE;
                work2->subState = 0;
                return 1;
            case 4:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
            case 5:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 0xE;
                work2->subState     = 0;
                work->pendingAction = 0;
                return 1;
        }
        work->pendingAction = 0;
        return 0;
    }
}
