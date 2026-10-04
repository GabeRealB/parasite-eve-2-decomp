/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Blends body slots 1-17 to the requested clip `animClip` over `animBlend`
/// frames at step `animStep` (and clears the blend), or, when that clip is
/// already playing, only sets the step - the Ivory build blends again
/// (`STALKER_ZEBRA_IVORY_REBLEND_SAME_CLIP`). Latches the clip into `animPlaying`.
void stalkerZebraIvoryBlendClip(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    s32                    i;

    work = (StalkerZebraIvoryWork*)arg0->work;
    if (work->animPlaying == work->animClip) {
        i = 1;
        do {
            work->rig.slots[i].rate = work->animStep;
#if STALKER_ZEBRA_IVORY_REBLEND_SAME_CLIP
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animClip, 0, work->animBlend);
#endif
            i++;
        } while (i < ARRAY_SIZE(work->rig.slots));
    } else {
        i = 1;
        do {
            work->rig.slots[i].rate = work->animStep;
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animClip, 0, work->animBlend);
            i++;
        } while (i < ARRAY_SIZE(work->rig.slots));
        work->animBlend = 0;
    }
    work->animPlaying = work->animClip;
}
