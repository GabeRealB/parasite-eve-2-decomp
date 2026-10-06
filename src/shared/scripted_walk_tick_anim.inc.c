/* Part of the scripted walk library; see scripted_walk.h. */

/// Advances the published walker's non-root animation slots and applies their poses.
///
/// The published work block, its bound model and clip data must stay live.
/// Slots 1 through 19 must have been reset or seeded for blended playback;
/// each consumes its configured rate in sixteenths of a frame, subject to
/// the death-playback adjustment. Slot 0 is the separately controlled root.
void SCRIPTED_WALK_TICK_ANIM(void)
{
    enum { SCRIPTED_WALK_FIRST_CHILD_SLOT = 1 };
    s32 slotIndex;

    slotIndex = SCRIPTED_WALK_FIRST_CHILD_SLOT;
    do {
        animationTickSlot(&gScriptedWalkWork->rig.anim, slotIndex);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(gScriptedWalkWork->rig.slots));
}
