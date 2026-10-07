/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Unless a pending action takes over, rests in state 0xA once the clip is
/// done; either way rolls a fresh `countdown` of 0x1E..0x9D frames.
void stalkerZebraIvoryWaitClipThenRest(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    StalkerZebraIvoryWork* work2;
    u32                    rnd;

    work = (StalkerZebraIvoryWork*)arg0->work;
    if ((_stalkerZebraIvoryApplyPendingReaction(arg0) << 0x10) != 0) {
        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        work->countdown = ((rnd >> 0x10) & 0x7F) + 0x1E;
    } else if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        work->countdown = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work2           = (StalkerZebraIvoryWork*)arg0->work;
        work2->state    = 0xA;
        work2->subState = 0;
    }
}
