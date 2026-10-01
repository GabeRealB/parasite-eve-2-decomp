/* Part of the Generator library; see generator.h. */

/// Pose tick. When the pose asked for (`field_320`) differs from the one the
/// animation slots were last queued for (`field_322`), slots 1-9 are re-queued
/// with it and its entry of `gGeneratorPoseStartFrames`, and the frame count
/// `field_324` restarts; otherwise every slot is ticked and the count advances
/// by one.
void generatorTickPose(Task* arg0)
{
    GeneratorWork* work;
    s32            i;
    s32            value;

    work = arg0->work;
    if ((s16)work->field_320 != work->field_322) {
        work->field_322 = work->field_320;
        work->field_324 = 0;
        value           = gGeneratorPoseStartFrames[(s16)work->field_320];
        for (i = 1; i < 10; i++) {
            func_800B4114(&work->anim, i, (s16)work->field_320, 0, value);
        }
    } else {
        work->field_324++;
        for (i = 1; i < 10; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}
