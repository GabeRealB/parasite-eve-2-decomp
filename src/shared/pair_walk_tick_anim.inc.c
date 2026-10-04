/* Part of the pair walk library; see pair_walk.h. */

/// Ticks animation slots 1..0x12.
void pairWalkTickAnim(Task* task)
{
    PairWalkWork* work;
    s32           i;

    work = task->work;
    i    = 1;
    do {
        animationTickSlot(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}
