/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Returns 1 when any of bits 0, 1 or 8 of the second animation slot's
/// `slots[1].flags` is set (reached a boundary, followed a control record, or
/// holding the boundary pose, `ANIMATION_SLOT_SETTLED`) and 0 otherwise. Bit 0
/// is read as a halfword and the other two through the word starting there,
/// which is why the work block is seen through `ActorsShared8013a0b0Work`.
s32 stalkerZebraIvoryClipDone(Task* arg0)
{
    ActorsShared8013a0b0Work* work = (ActorsShared8013a0b0Work*)arg0->work;

    if ((work->flags_FC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_FC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}
