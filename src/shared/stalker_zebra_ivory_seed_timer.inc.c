/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Sets the attack-choice timer to a base plus two random spreads, in update ticks.
///
/// Adds successive 0..63 and 0..15 draws and advances the shared LCG twice;
/// storage narrows the sum to s16. Zebra accepts an s32 base. Ivory accepts
/// an s16 base and zero stops the timer without consuming either random draw.
static void _stalkerZebraIvorySeedTimer(Task* task, STALKER_ZEBRA_IVORY_TIMER_BASE baseFrames)
{
#if STALKER_ZEBRA_IVORY_TIMER_ZERO_STOPS
    StalkerZebraIvoryWork* work;
#endif
    enum { STALKER_ZEBRA_IVORY_TIMER_WIDE_SPREAD_MASK   = 0x3F,
           STALKER_ZEBRA_IVORY_TIMER_NARROW_SPREAD_MASK = 0xF };
    u32 wideSpreadState;
    u32 narrowSpreadState;

#if STALKER_ZEBRA_IVORY_TIMER_ZERO_STOPS
    work = (StalkerZebraIvoryWork*)task->work;
    if (baseFrames != 0) {
        wideSpreadState   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        narrowSpreadState = (wideSpreadState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState   = narrowSpreadState;
        work->timer       = baseFrames + ((wideSpreadState >> 0x10) & STALKER_ZEBRA_IVORY_TIMER_WIDE_SPREAD_MASK) + ((narrowSpreadState >> 0x10) & STALKER_ZEBRA_IVORY_TIMER_NARROW_SPREAD_MASK);
        return;
    }
    work->timer = 0;
#else
    wideSpreadState                             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    narrowSpreadState                           = (wideSpreadState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState                             = narrowSpreadState;
    ((StalkerZebraIvoryWork*)task->work)->timer = baseFrames + ((wideSpreadState >> 0x10) & STALKER_ZEBRA_IVORY_TIMER_WIDE_SPREAD_MASK) + ((narrowSpreadState >> 0x10) & STALKER_ZEBRA_IVORY_TIMER_NARROW_SPREAD_MASK);
#endif
}
