/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Turns the pending action into a state change - light recoil to state 3,
/// heavy recoil and blast to state 4, status hold to state 5 and knockdown to
/// state 0xF, or on the ceiling status hold and knockdown to state 0xE - and
/// clears it. Returns 1 when it changed state.
s32 stalkerZebraIvoryTakePending(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    StalkerZebraIvoryWork* work2;

    work = (StalkerZebraIvoryWork*)arg0->work;
    if (work->onCeiling == 0) {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 3;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 5;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 0xF;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        return 0;
    } else {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 3;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                work2           = (StalkerZebraIvoryWork*)arg0->work;
                work2->state    = 0xE;
                work2->subState = 0;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                work2               = (StalkerZebraIvoryWork*)arg0->work;
                work2->state        = 0xE;
                work2->subState     = 0;
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        return 0;
    }
}
