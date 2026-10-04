/* Part of the paced walk library; see paced_walk.h. */

/// Reseeds animation slots 1..0x13 with `animId`, each blending from the pose
/// it holds over `blendFrames` frames, and records that id as the one applied.
void pacedWalkBlendAnim(Task* task)
{
    PacedWalkWork* work;
    s32            i;

    work = task->work;
    i    = 1;
    do {
        animationSeekSlotWithBlend(&work->rig.anim, i, work->st.animId, 0, work->blendFrames);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}
