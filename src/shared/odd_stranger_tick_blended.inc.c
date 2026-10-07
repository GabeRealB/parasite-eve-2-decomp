/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Ticks both rigs and mixes the first ten parts' rotations.
///
/// Requires initialized work with both rigs bound to the same live model and
/// loaded sets. Slots 1..10 sample both rigs; slots 11..18 tick only primary.
/// Primary rate is `animRate - 3`, secondary rate is `blendRate`, in sixteenths
/// of a frame per call. `blendWeight` is the primary rotation's Q12 weight,
/// normally 0..`ONE`; secondary takes its complement. Translation stays primary.
/// The sampled stack poses live through each blend call. Reverse playback
/// requires bank-backed current endpoints, as `animationTickSlotPose` specifies.
static void _oddStrangerTickBlendedAnimation(Task* task)
{
    AnimationPose     primaryPose;
    AnimationPose     blendPose;
    AnimationContext* primaryContext;
    s16               primaryWeight;
    s16               slotIndex;
    OddStrangerWork*  work;

    work           = task->work;
    primaryWeight  = work->blendWeight;
    primaryContext = &work->rig.anim;
    for (slotIndex = ODD_STRANGER_FIRST_ANIMATED_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (slotIndex <= ODD_STRANGER_LAST_BLENDED_SLOT) {
            work->blend.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate   = (work->animRate - 3);
            animationTickSlotPose(primaryContext, slotIndex, &primaryPose, NULL);
            animationTickSlotPose(&work->blend.anim, slotIndex, &blendPose, NULL);
            animationApplyPoseWithBlendedRotation(primaryContext, slotIndex, &primaryPose, &blendPose, primaryWeight, ONE - primaryWeight);
        } else {
            work->rig.slots[slotIndex].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}
