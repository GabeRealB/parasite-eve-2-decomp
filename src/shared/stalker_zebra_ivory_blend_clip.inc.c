/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Reseeds every body track with the requested rate and blend from frame zero.
///
/// `workArg` must be a side-effect-free pointer to the initialized carrier work;
/// `slotIndexArg` must be a writable s32 local used only as the loop index here.
/// Both expressions are evaluated repeatedly. Invoke as a standalone phase inside
/// explicit braces; the expansion contains multiple statements and captures no
/// other caller locals.
#define STALKER_ZEBRA_IVORY_BLEND_BODY_TRACKS(workArg, slotIndexArg)                                                    \
    (slotIndexArg) = 1;                                                                                                 \
    do {                                                                                                                \
        (workArg)->rig.slots[(slotIndexArg)].rate = (workArg)->animStep;                                                \
        animationSeekSlotWithBlend(&(workArg)->rig.anim, (slotIndexArg), (workArg)->animClip, 0, (workArg)->animBlend); \
        (slotIndexArg)++;                                                                                               \
    } while ((slotIndexArg) < ARRAY_SIZE((workArg)->rig.slots))

/// Applies the requested body clip with a transition measured in normal-rate frames.
///
/// Drives slots 1..17 of an initialized live rig and latches `animPlaying`.
/// `animClip` must select a loaded set supporting every body track; `animStep`
/// narrows to the slots' signed byte rate (16 is one normal frame per tick).
/// Changing clip consumes `animBlend`. Repeating the clip preserves that duration:
/// Zebra changes only the rate, while Ivory also reseeds the blend.
static void _stalkerZebraIvoryBlendClip(Task* task)
{
    StalkerZebraIvoryWork* work;
    s32                    slotIndex;

    work = (StalkerZebraIvoryWork*)task->work;
    if (work->animPlaying == work->animClip) {
#if STALKER_ZEBRA_IVORY_REBLEND_SAME_CLIP
        STALKER_ZEBRA_IVORY_BLEND_BODY_TRACKS(work, slotIndex);
#else
        slotIndex = 1;
        do {
            work->rig.slots[slotIndex].rate = work->animStep;
            slotIndex++;
        } while (slotIndex < ARRAY_SIZE(work->rig.slots));
#endif
    } else {
        STALKER_ZEBRA_IVORY_BLEND_BODY_TRACKS(work, slotIndex);
        work->animBlend = 0;
    }
    work->animPlaying = work->animClip;
}

#undef STALKER_ZEBRA_IVORY_BLEND_BODY_TRACKS
