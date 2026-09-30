/* Part of the paced walk library; see paced_walk.h. */

/// Reseeds animation slots 1..0x13 with `animId`, each at rate 1, and records
/// that id as the one applied.
void pacedWalkResetAnim(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}
