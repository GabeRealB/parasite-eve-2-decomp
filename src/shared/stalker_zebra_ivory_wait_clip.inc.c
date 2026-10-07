/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Unless a pending action takes over, returns to state 2 once the clip is
/// done.
void stalkerZebraIvoryWaitClip(Task* arg0)
{
    StalkerZebraIvoryWork* work;

    if (((_stalkerZebraIvoryApplyPendingReaction(arg0) << 0x10) == 0) && ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0)) {
        work           = (StalkerZebraIvoryWork*)arg0->work;
        work->state    = 2;
        work->subState = 0;
    }
}
