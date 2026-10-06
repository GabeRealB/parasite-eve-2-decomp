/* Part of the scripted walk library; see scripted_walk.h. */

static void SCRIPTED_WALK_TICK_ANIM(void)
{
    enum { SCRIPTED_WALK_FIRST_CHILD_SLOT = 1 };
    s32 slotIndex;

    for (slotIndex = SCRIPTED_WALK_FIRST_CHILD_SLOT; slotIndex < ARRAY_SIZE(SCRIPTED_WALK_WORK->rig.slots); slotIndex++) {
        animationTickSlot(&SCRIPTED_WALK_WORK->rig.anim, slotIndex);
    }
}
