/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Returns 1 when any of bits 0, 1 or 8 of the second animation slot's
/// `rig.slots[1].status` is set (reached a boundary, followed a control record, or
/// holding the boundary pose, `ANIMATION_SLOT_SETTLED`) and 0 otherwise. Bit 0
/// is read as a halfword and the other two through the word starting there
/// (`AnimationSlot::status`).
s32 stalkerZebraIvoryClipDone(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}
