/* Part of the power plant pod library; see power_plant_pod.h. */

/// Pose tick. When the pose asked for (`field_320`) differs from the one the
/// animation slots were last queued for (`field_322`), slots 1-9 are re-queued
/// with it and its entry of `gPodPoseStartFrames`, and the frame count
/// `field_324` restarts; otherwise every slot is ticked and the count advances
/// by one.
void podTickPose(Task* arg0)
{
    Actor05300Work* work;
    s32             i;
    s32             value;

    work = arg0->work;
    if ((s16)work->field_320 != work->field_322) {
        work->field_322 = work->field_320;
        work->field_324 = 0;
        value           = gPodPoseStartFrames[(s16)work->field_320];
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
