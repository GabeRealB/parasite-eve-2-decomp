/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Advances the two animation rigs and mixes secondary rotation into slots 1..10.
///
/// The cutscene build uses `blendWeight` for the main rotation and its
/// 4096 complement for the secondary rotation on slots 1..10. All slots 1..17
/// advance the main rig at `animRate - 3`; the blended slots also advance the
/// secondary rig at `blendRate`. Rates use sixteenths of a frame, and signed
/// negative rates are retained. Both rigs and their borrowed clip data must be live.
static void _desertChaserBlendTick(Task* task)
{
    AnimationPose     mainPose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               mainWeight;
    s16               slotIndex;
    DesertChaserWork* work;

    work       = task->work;
    mainWeight = work->blendWeight;
    anim       = &work->rig.anim;
    // Leave slot 0 intact; only slots 1..10 mix the secondary rotation.
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (slotIndex < DESERT_CHASER_BLEND_FIRST_UNBLENDED_SLOT) {
            work->blend.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate   = (work->animRate - DESERT_CHASER_BLEND_MAIN_RATE_BIAS);
            animationTickSlotPose(anim, slotIndex, &mainPose, NULL);
            animationTickSlotPose(&work->blend.anim, slotIndex, &blendPose, NULL);
            animationApplyPoseWithBlendedRotation(anim, slotIndex, &mainPose, &blendPose, mainWeight, DESERT_CHASER_BLEND_WEIGHT_ONE - mainWeight);
        } else {
            work->rig.slots[slotIndex].rate = (work->animRate - DESERT_CHASER_BLEND_MAIN_RATE_BIAS);
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}
