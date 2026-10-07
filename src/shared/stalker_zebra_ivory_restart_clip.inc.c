/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Restarts body tracks 1..17 at the requested clip's beginning and latches it as playing.
///
/// Requires an initialized live rig and a loaded `animClip` with every body
/// track. Applies `animStep` through the slots' signed byte rate; slot 0 is
/// untouched. Does not change the request or frame counter.
static void _stalkerZebraIvoryRestartClip(Task* task)
{
    StalkerZebraIvoryWork* work;
    s32                    slotIndex;

    work      = (StalkerZebraIvoryWork*)task->work;
    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animStep;
        animationResetSlot(&work->rig.anim, slotIndex, work->animClip);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
    work->animPlaying = work->animClip;
}
