/* Part of the Diver library; see diver.h. */

/// Restarts every body slot (1-14) on the requested clip `animClip` at step
/// `animStep`, and latches the clip into `animPlaying`.
void diverRestartClip(Task* arg0)
{
    DiverWork* work;
    s32        i;

    work = arg0->work;
    i    = 1;
    do {
        animationResetSlot(&work->anim, i, work->animClip);
        work->slots[i].rate = work->animStep;
        i++;
    } while (i < ARRAY_SIZE(work->slots));
    work->animPlaying = (u16)work->animClip;
}
