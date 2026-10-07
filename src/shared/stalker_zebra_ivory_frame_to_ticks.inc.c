/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Converts a normal-rate frame count to ticks at the requested animation rate.
///
/// Returns 0 for zero `animStep`. Division uses eight fractional bits before
/// dropping four to account for the sixteenth-frame rate, then narrows to s16.
/// For nonnegative frames and a positive rate this rounds down; callers supply
/// counts that fit the signed-halfword result. Negative inputs retain the same
/// signed division, shift and truncation behavior.
static s16 _stalkerZebraIvoryFrameToTicks(Task* task, s16 normalFrames)
{
    enum { STALKER_ZEBRA_IVORY_FRAME_FRACTION_BITS = 8,
           STALKER_ZEBRA_IVORY_RATE_FRACTION_BITS  = 4 };
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)task->work;

    if (work->animStep == 0) {
        return 0;
    }
    return (s16)(((normalFrames << STALKER_ZEBRA_IVORY_FRAME_FRACTION_BITS) / work->animStep) >> STALKER_ZEBRA_IVORY_RATE_FRACTION_BITS);
}
