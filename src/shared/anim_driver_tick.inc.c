/* Part of the animation driver library; see anim_driver.h. */

/// Restarts animation slots 1 to 5 on the requested animation, at the rate
/// `rate + rateBias`, and records it as the one playing.
static __inline__ void animDriverResetSlots(AnimDriverWork* arg0)
{
    AnimDriverWork* work = arg0;
    s32             i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->rate + work->rateBias;
        animationResetSlot(&work->anim, i, work->requested);
    }
    work->playing = work->requested;
}

/// Advances animation slots 1 to 5 by one frame at the rate `rate + rateBias`.
static __inline__ void animDriverTickSlots(Task* arg0)
{
    AnimDriverWork* work;
    s32             i;

    work = arg0->work;
    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->rate + work->rateBias;
        animationTickSlot(&work->anim, i);
    }
}

/// Motion driver the state handlers run every frame. A restart request in
/// `motion` (1 or 2) restarts the slots and clears the frame counters; while
/// running (3) it advances the slots, counts `frame` and, while bit 1 of slot
/// 1's flags is set, `cueFrames` as well.
void animDriverTick(Task* arg0)
{
    AnimDriverWork* work;

    work = arg0->work;
    if (work->motion == 1) {
        animDriverResetSlots(work);
        work->motion    = 3;
        work->frame     = 0;
        work->cueFrames = 0;
    } else if (work->motion == 2) {
        animDriverResetSlots(work);
        work->motion    = 3;
        work->frame     = 0;
        work->cueFrames = 0;
    } else if (work->motion == 3) {
        work->frame++;
        animDriverTickSlots(arg0);
        if (work->slots[1].flags & 2) {
            work->cueFrames++;
        }
    }
}
