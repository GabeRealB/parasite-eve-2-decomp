/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Seeds `timer` with `arg1` plus two random spreads (0..63 and 0..15); in
/// the Ivory build a zero base stops the timer instead
/// (`STALKER_ZEBRA_IVORY_TIMER_ZERO_STOPS`).
void stalkerZebraIvorySeedTimer(Task* arg0, STALKER_ZEBRA_IVORY_TIMER_BASE arg1)
{
#if STALKER_ZEBRA_IVORY_TIMER_ZERO_STOPS
    StalkerZebraIvoryWork* work;
#endif
    u32 rnd1;
    u32 rnd2;

#if STALKER_ZEBRA_IVORY_TIMER_ZERO_STOPS
    work = (StalkerZebraIvoryWork*)arg0->work;
    if ((arg1 << 16) != 0) {
#endif
        rnd1            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        rnd2            = (rnd1 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd2;
#if !STALKER_ZEBRA_IVORY_TIMER_ZERO_STOPS
        ((StalkerZebraIvoryWork*)arg0->work)->timer = arg1 + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
#else
    work->timer = arg1 + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
    return;
}
work->timer = 0;
#endif
    }
