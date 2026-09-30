/* Part of the footstep walk library; see footstep_walk.h. */

/// Ticks animation slots 1..0x12 of the enemy's animation context.
void footstepWalkTickAnim(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&gFootstepWalkWork->rig.anim, i);
        i++;
    } while (i < 0x13);
}
