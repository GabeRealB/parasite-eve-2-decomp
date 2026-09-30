/* Part of the patrol walker library; see patrol_walker.h. */

/// Per-tick walker step: advances the animation the `field_68` byte selects,
/// resolves the one-based character ID in `field_6E` against `Player_Status`,
/// and ramp-scales the model matrix between `field_5E` and `field_5C`. The
/// working frame is carved off the scratch stack and handed back once the
/// coordinate has been rebuilt. Same body as the acropolis bridge room's
/// `func_acropolis_bridge_8018532C`.
void patrolWalkerTick(OverlayWalker* walker)
{
    u8*                       head;
    OverlayWalkerTickScratch* block;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x28;
    block                    = SCRATCH_STACK_CURSOR(OverlayWalkerTickScratch);
    patrolWalkerStep(walker, head, block);
    walker->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}
