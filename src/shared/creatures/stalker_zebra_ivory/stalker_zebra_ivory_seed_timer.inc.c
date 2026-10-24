/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Sets the attack-choice timer to a base plus two random spreads, in update ticks.
///
/// Adds successive 0..63 and 0..15 draws and advances the shared LCG twice;
/// storage narrows the sum to s16. Zebra accepts an s32 base. Ivory accepts
/// an s16 base and zero stops the timer without consuming either random draw.
static void _stalkerZebraIvorySeedTimer(Task* task, STALKER_ZEBRA_IVORY_TIMER_BASE_TYPE baseFrames)
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
        wideSpreadState   = RANDOM_LCG_NEXT(gRandomLcgState);
        narrowSpreadState = RANDOM_LCG_NEXT(wideSpreadState);
        gRandomLcgState   = narrowSpreadState;
        work->timer       = baseFrames + ((wideSpreadState >> 0x10) & STALKER_ZEBRA_IVORY_TIMER_WIDE_SPREAD_MASK) + ((narrowSpreadState >> 0x10) & STALKER_ZEBRA_IVORY_TIMER_NARROW_SPREAD_MASK);
        return;
    }
    work->timer = 0;
#else
    wideSpreadState                             = RANDOM_LCG_NEXT(gRandomLcgState);
    narrowSpreadState                           = RANDOM_LCG_NEXT(wideSpreadState);
    gRandomLcgState                             = narrowSpreadState;
    ((StalkerZebraIvoryWork*)task->work)->timer = baseFrames + ((wideSpreadState >> 0x10) & STALKER_ZEBRA_IVORY_TIMER_WIDE_SPREAD_MASK) + ((narrowSpreadState >> 0x10) & STALKER_ZEBRA_IVORY_TIMER_NARROW_SPREAD_MASK);
#endif
}

/// Disables room-grid testing for the probe capsule while keeping its body linked.
static void _stalkerZebraIvoryDisableCapsuleGrid(Task* task)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)task->work;

    work->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
}
