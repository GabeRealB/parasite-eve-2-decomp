/* Part of the Glutton library; see glutton.h. */

/// Ticks both pose sources and mixes their rotations onto the three driving rigs.
///
/// Requires initialized host and escort-0/1 rigs with loaded clip data. Host slots
/// 1..7 and escort slots 0..3 capture driving and secondary poses at rates
/// `animRate - 3` and `blendRate`, in sixteenths of a frame per tick. Translation
/// comes from the driving pose; rotation weights are `blendWeight` for the driving
/// pose and `ONE - blendWeight` for the secondary pose. Weights normally lie in
/// 0..ONE. Pose capture, blending and nested conversions require initialized
/// scratch and overwrite GTE state; no pose pointer is retained.
static void _gluttonTickBlended(Task* task)
{
    enum { GLUTTON_HOST_BLENDED_SLOT_END = 11 };
    AnimationPose drivingPose;
    AnimationPose secondaryPose;
    GluttonWork*  work            = task->work;
    s32           drivingWeight   = work->blendWeight;
    s32           secondaryWeight = ONE - drivingWeight;
    s16           slotIndex;

    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->hostRig.slots); slotIndex++) {
        // All host slots use the blended side of this ten-slot split.
        if (slotIndex < GLUTTON_HOST_BLENDED_SLOT_END) {
            work->hostBlendRig.slots[slotIndex].rate = work->blendRate;
            work->hostRig.slots[slotIndex].rate      = work->animRate - 3;
            animationTickSlotPose(&work->hostRig.anim, slotIndex, &drivingPose, NULL);
            animationTickSlotPose(&work->hostBlendRig.anim, slotIndex, &secondaryPose, NULL);
            animationApplyPoseWithBlendedRotation(&work->hostRig.anim, slotIndex, &drivingPose, &secondaryPose, drivingWeight, secondaryWeight);
        } else {
            work->hostRig.slots[slotIndex].rate = work->animRate - 3;
            animationTickSlot(&work->hostRig.anim, slotIndex);
        }
    }

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->escort0Rig.slots); slotIndex++) {
        work->escort0BlendRig.slots[slotIndex].rate = work->blendRate;
        work->escort0Rig.slots[slotIndex].rate      = work->animRate - 3;
        animationTickSlotPose(&work->escort0Rig.anim, slotIndex, &drivingPose, NULL);
        animationTickSlotPose(&work->escort0BlendRig.anim, slotIndex, &secondaryPose, NULL);
        animationApplyPoseWithBlendedRotation(&work->escort0Rig.anim, slotIndex, &drivingPose, &secondaryPose, drivingWeight, secondaryWeight);
    }

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->escort1Rig.slots); slotIndex++) {
        work->escort1BlendRig.slots[slotIndex].rate = work->blendRate;
        work->escort1Rig.slots[slotIndex].rate      = work->animRate - 3;
        animationTickSlotPose(&work->escort1Rig.anim, slotIndex, &drivingPose, NULL);
        animationTickSlotPose(&work->escort1BlendRig.anim, slotIndex, &secondaryPose, NULL);
        animationApplyPoseWithBlendedRotation(&work->escort1Rig.anim, slotIndex, &drivingPose, &secondaryPose, drivingWeight, secondaryWeight);
    }
}
