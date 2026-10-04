/* Part of the paced walk library; see paced_walk.h. */

/// Ticks animation slots 1..0x13 of the actor's animation context.
void pacedWalkTickAnim(Task* task)
{
    PACED_WALK_WORK_T* work;
    s32                i;

    work = task->work;
    i    = 1;
    do {
        animationTickSlot(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}
