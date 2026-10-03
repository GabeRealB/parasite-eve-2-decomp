/* Part of the animation driver library; see anim_driver.h. */

/// Restarts animation slots 1 to 5 on the requested set and records it as the
/// one playing.
///
/// The combined rate stored ahead of each reset does not survive it: the
/// reset puts the slot back at `ANIMATION_RATE_ONE`, and the sum takes effect
/// when the first advance stores it again.
static __inline__ void animDriverResetSlots(AnimDriverWork* arg0)
{
    AnimDriverWork* work = arg0; // The carriers match only with this copy of the parameter
    s32             i;

    for (i = ANIM_DRIVER_FIRST_SLOT; i < ARRAY_SIZE(work->rig.slots); i++) {
        work->rig.slots[i].rate = work->rate + work->rateBias;
        animationResetSlot(&work->rig.anim, i, work->requestedSet);
    }
    work->playingSet = work->requestedSet;
}

/// Advances animation slots 1 to 5 by one tick at the rate `rate + rateBias`.
static __inline__ void animDriverTickSlots(Task* arg0)
{
    AnimDriverWork* work;
    s32             i;

    work = arg0->work;
    for (i = ANIM_DRIVER_FIRST_SLOT; i < ARRAY_SIZE(work->rig.slots); i++) {
        work->rig.slots[i].rate = work->rate + work->rateBias;
        animationTickSlot(&work->rig.anim, i);
    }
}

/// Animation driver the state handlers run every frame. A restart request in
/// `state` restarts the slots on `requestedSet` and clears both counters;
/// while playing it advances the slots, counts the tick in `tickCount` and,
/// when the first driven slot followed a control jump, in `jumpCount` as well.
void animDriverTick(Task* arg0)
{
    AnimDriverWork* work;

    work = arg0->work;
    if (work->state == ANIM_DRIVER_STATE_RESTART_1) {
        animDriverResetSlots(work);
        work->state     = ANIM_DRIVER_STATE_PLAYING;
        work->tickCount = 0;
        work->jumpCount = 0;
    } else if (work->state == ANIM_DRIVER_STATE_RESTART_2) {
        animDriverResetSlots(work);
        work->state     = ANIM_DRIVER_STATE_PLAYING;
        work->tickCount = 0;
        work->jumpCount = 0;
    } else if (work->state == ANIM_DRIVER_STATE_PLAYING) {
        work->tickCount++;
        animDriverTickSlots(arg0);
        if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            work->jumpCount++;
        }
    }
}
