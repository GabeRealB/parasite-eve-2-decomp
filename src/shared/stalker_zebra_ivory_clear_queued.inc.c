/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Clears the queued mode and its flag.
void stalkerZebraIvoryClearQueued(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    work->queuedFlag = 0;
    work->queuedMode = 0;
}
