/* Part of the pair walk library; see pair_walk.h. */

/// Ticks animation slots 1..0x12.
void pairWalkTickAnim(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}
