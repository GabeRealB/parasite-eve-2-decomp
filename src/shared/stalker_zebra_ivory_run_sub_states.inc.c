/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Folds both arms away, then, unless an armed pending action takes over,
/// runs the sub-state handler `gStalkerZebraIvorySubStates` gives for
/// `subState`.
void stalkerZebraIvoryRunSubStates(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;
    TaskFuncTable3         fns  = gStalkerZebraIvorySubStates;

    stalkerZebraIvoryClearQueued(arg0);
    if ((stalkerZebraIvoryTakeArmedPending(arg0) << 0x10) == 0) {
        fns.funcs[work->subState](arg0);
    }
}
