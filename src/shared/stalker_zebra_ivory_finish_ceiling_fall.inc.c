/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Seeds the time spent crawling on the back from one shared random draw.
///
/// Advances the unsigned 32-bit LCG once; the stored duration is 30..157 ticks.
static __inline__ void _stalkerZebraIvorySeedCrawlCountdown(StalkerZebraIvoryWork* work)
{
    enum { STALKER_ZEBRA_IVORY_CRAWL_BASE_TICKS  = 30,
           STALKER_ZEBRA_IVORY_CRAWL_SPREAD_MASK = 127 };
    u32 randomState;

    randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState = randomState;
    work->countdown = ((randomState >> 16) & STALKER_ZEBRA_IVORY_CRAWL_SPREAD_MASK) + STALKER_ZEBRA_IVORY_CRAWL_BASE_TICKS;
}

/// Finishes a ceiling fall by starting the timed crawl on the Stalker's back.
///
/// Pending reactions take priority. Either a taken reaction or clip completion
/// draws a fresh 30..157-update-tick countdown; only completion without a reaction
/// selects the crawl behavior and resets `subState`. Waiting changes neither the
/// countdown nor random state. Does not tick animation or decrement countdown.
static void _stalkerZebraIvoryFinishCeilingFall(Task* task)
{
    StalkerZebraIvoryWork* work = task->work;

    if ((s16)_stalkerZebraIvoryApplyPendingReaction(task) != 0) {
        _stalkerZebraIvorySeedCrawlCountdown(work);
    } else if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _stalkerZebraIvorySeedCrawlCountdown(work);
        _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_CRAWL_ON_BACK);
    }
}
