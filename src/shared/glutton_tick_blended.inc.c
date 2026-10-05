/* Part of the Glutton library; see glutton.h. */

/// Advance the host's and escort 0's and 1's rigs with each blend rig mixed in
/// at weight `blendWeight`: every slot of a blend rig ticks at `blendRate`,
/// every slot of a driving rig at `animRate` less 3, and the pose written to
/// the driving rig is the mix of the two.
void gluttonTickBlended(Task* arg0)
{
    AnimationPose pose0;
    AnimationPose pose1;
    GluttonWork*  work     = arg0->work;
    s32           blend    = work->blendWeight;
    s32           invBlend = 0x1000 - blend;
    s16           i;

    for (i = 1; i < 8; i++) {
        if (i < 11) {
            work->hostBlendRig.slots[i].rate = work->blendRate;
            work->hostRig.slots[i].rate      = work->animRate - 3;
            animationTickSlotPose(&work->hostRig.anim, i, &pose0, 0);
            animationTickSlotPose(&work->hostBlendRig.anim, i, &pose1, 0);
            animationApplyPoseWithBlendedRotation(&work->hostRig.anim, i, &pose0, &pose1, blend, invBlend);
        } else {
            work->hostRig.slots[i].rate = work->animRate - 3;
            animationTickSlot(&work->hostRig.anim, i);
        }
    }

    for (i = 0; i < 4; i++) {
        work->escort0BlendRig.slots[i].rate = work->blendRate;
        work->escort0Rig.slots[i].rate      = work->animRate - 3;
        animationTickSlotPose(&work->escort0Rig.anim, i, &pose0, 0);
        animationTickSlotPose(&work->escort0BlendRig.anim, i, &pose1, 0);
        animationApplyPoseWithBlendedRotation(&work->escort0Rig.anim, i, &pose0, &pose1, blend, invBlend);
    }

    for (i = 0; i < 4; i++) {
        work->escort1BlendRig.slots[i].rate = work->blendRate;
        work->escort1Rig.slots[i].rate      = work->animRate - 3;
        animationTickSlotPose(&work->escort1Rig.anim, i, &pose0, 0);
        animationTickSlotPose(&work->escort1BlendRig.anim, i, &pose1, 0);
        animationApplyPoseWithBlendedRotation(&work->escort1Rig.anim, i, &pose0, &pose1, blend, invBlend);
    }
}
