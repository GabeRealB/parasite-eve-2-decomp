/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Per-frame blended animation tick: for pose slots 1..10 it sets both
/// contexts' slot rates (the blend context's from `blendRate`, the pose
/// context's three below `animRate`), samples each context's pose and writes
/// their mix weighted by `blendWeight` against its 0x1000 complement. Slots 11
/// and up only take the pose rate and are advanced unblended.
void oddStrangerTickBlended(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    OddStrangerWork*  work;

    work   = arg0->work;
    weight = work->blendWeight;
    anim   = &work->rig.anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blend.slots[i].rate = work->blendRate;
            work->rig.slots[i].rate   = (work->animRate - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blend.anim, i, &blendPose, 0);
            animationApplyPoseWithBlendedRotation(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->rig.slots[i].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, i);
        }
    }
}
