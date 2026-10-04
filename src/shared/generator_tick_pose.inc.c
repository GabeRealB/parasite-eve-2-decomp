/* Part of the Generator library; see generator.h. */

/// Pose tick. When the pose asked for (`animSet`) differs from the one the
/// animation slots were last queued for (`appliedAnimSet`), slots 1-9 are re-queued
/// with it and its entry of `gGeneratorPoseStartFrames`, and the frame count
/// `animFrames` restarts; otherwise every slot is ticked and the count advances
/// by one.
void generatorTickPose(Task* arg0)
{
    GeneratorWork* work;
    s32            i;
    s32            value;

    work = arg0->work;
    if (work->animSet != work->appliedAnimSet) {
        work->appliedAnimSet = work->animSet;
        work->animFrames     = 0;
        value                = gGeneratorPoseStartFrames[work->animSet];
        for (i = 1; i < ARRAY_SIZE(work->slots); i++) {
            animationSeekSlotWithBlend(&work->anim, i, work->animSet, 0, value);
        }
    } else {
        work->animFrames++;
        for (i = 1; i < ARRAY_SIZE(work->slots); i++) {
            animationTickSlot(&work->anim, i);
        }
    }
}
