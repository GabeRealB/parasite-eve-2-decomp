/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Ticks the animation and moves to the next state once the clip is done.
void stalkerZebraIvoryAnimateUntilDone(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    _stalkerZebraIvoryTickAnim(arg0);
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->state = work->state + 1;
    }
}
