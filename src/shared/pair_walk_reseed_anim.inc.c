/* Part of the pair walk library; see pair_walk.h. */

/// Reseeds animation slots 1..0x12 with clip `animId`, blended over `blendFrames`,
/// and records the clip as the applied one.
void pairWalkReseedAnim(Task* task)
{
    PairWalkWork* work;
    s32           i;

    work = task->work;
    i    = 1;
    do {
        animationSeekSlotWithBlend(&work->rig.anim, i, work->st.animId, 0, work->blendFrames);
        i++;
    } while (i < 0x13);
    work->st.appliedAnimId = work->st.animId;
}
