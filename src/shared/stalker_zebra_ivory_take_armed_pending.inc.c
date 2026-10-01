/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// When the pending action is armed and the Stalker is off the ceiling, takes
/// actions 3 (state 5) and 5 (state 0xF); always clears the pending action.
/// Returns 1 when it changed state.
s32 stalkerZebraIvoryTakeArmedPending(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    if (work->pendingArmed == 1 && work->onCeiling == 0) {
        switch (work->pendingAction) {
            case 3:
                work->state         = 5;
                work->subState      = 0;
                work->pendingAction = 0;
                return 1;
            case 5:
                work->state         = 0xF;
                work->subState      = 0;
                work->pendingAction = 0;
                return 1;
        }
    }
    work->pendingAction = 0;
    return 0;
}
