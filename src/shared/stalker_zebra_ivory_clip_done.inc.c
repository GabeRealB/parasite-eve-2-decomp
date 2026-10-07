/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Returns 1 when body slot 1 reaches a boundary, follows a jump or holds its end pose.
///
/// Returns 0 otherwise. Reads the live initialized slot's latest tick/walk
/// flags; a loop jump counts as completion for these state handlers.
static s32 _stalkerZebraIvoryClipDone(Task* task)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)task->work;

    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}
