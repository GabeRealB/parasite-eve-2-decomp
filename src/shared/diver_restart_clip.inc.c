/* Part of the Diver library; see diver.h. */

/// Restarts the requested clip on driven body slots 1..14.
///
/// Requires the live `DiverWork`, initialized rig and loaded clip bank.
/// `animClip` must support every driven track. Each reset selects its track's
/// start and gets `animStep` narrowed to a signed byte rate in sixteenths of a
/// frame. Latches `animPlaying`; preserves slot 0 and `animRequest` so the
/// driver can acknowledge the request. Borrowed playback storage stays live.
static void _diverRestartClip(Task* task)
{
    DiverWork* work;
    s32        slotIndex;

    work      = task->work;
    slotIndex = 1;
    do {
        animationResetSlot(&work->rig.anim, slotIndex, work->animClip);
        work->rig.slots[slotIndex].rate = work->animStep;
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
    work->animPlaying = work->animClip;
}
