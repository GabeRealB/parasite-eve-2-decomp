/* Part of the scripted walk library; see scripted_walk.h. */

/// Ticks animation slots 1..0x13 of the work block's animation context.
void scriptedWalkTickAnim(void)
{
    s32 i;

    i = 1;
    do {
        animationTickSlot(&gScriptedWalkWork->rig.anim, i);
        i++;
    } while (i < 0x14);
}
