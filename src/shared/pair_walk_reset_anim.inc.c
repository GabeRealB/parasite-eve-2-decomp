/* Part of the pair walk library; see pair_walk.h. */

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a reseed
/// argument, and records the clip as the applied one.
void pairWalkResetAnim(Task* task)
{
    PairWalkWork* work;
    s32           i;

    work = task->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = 1;
        animationResetSlot(&work->rig.anim, i, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}
