/* Part of the footstep walk library; see footstep_walk.h. */

/// Advances parts 1 through 18 of the published walker and writes their poses.
///
/// Sound and quiet carriers both bind `gFootstepWalkWork->rig.anim` to their
/// live nineteen-slot rig. Driven slots must be initialized, with loaded
/// tracks, coordinates, buffers and scratch/GTE state satisfying
/// `animationTickSlot`. Part 0 is controlled separately by movement and placement.
static void _footstepWalkTickAnim(void)
{
    s32 slotIndex;

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(gFootstepWalkWork->rig.slots); slotIndex++) {
        animationTickSlot(&gFootstepWalkWork->rig.anim, slotIndex);
    }
}
