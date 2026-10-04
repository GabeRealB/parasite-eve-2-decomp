/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Restarts body slots 1-17 on the requested clip `animClip` at step
/// `animStep`, and latches the clip into `animPlaying`.
void stalkerZebraIvoryRestartClip(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    s32                    i;

    work = (StalkerZebraIvoryWork*)arg0->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = work->animStep;
        animationResetSlot(&work->rig.anim, i, work->animClip);
        i++;
    } while (i < ARRAY_SIZE(work->rig.slots));
    work->animPlaying = work->animClip;
}
