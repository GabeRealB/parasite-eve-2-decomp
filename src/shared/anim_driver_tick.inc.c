/* Part of the animation driver library; see anim_driver.h. */

/// Initializes slots 1 to 5 on the requested set and records the selected set.
///
/// The rig must be bound to its own storage and a live model, with loaded
/// tracks and coordinates for all five indices. `requestedSet` must select
/// a loaded set, excluding `ANIMATION_SET_BUFFERED_POSE`. No pose is written;
/// slots finish at `ANIMATION_RATE_ONE` and driver counters are unchanged.
static __inline__ void _animDriverResetSlots(AnimDriverWork* driverWork)
{
    AnimDriverWork* work = driverWork; // The carriers match only with this copy of the parameter
    s32             slotIndex;

    // Reset replaces this rate with normal speed; advancing installs the sum again.
    for (slotIndex = ANIM_DRIVER_FIRST_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->rate + work->rateBias;
        animationResetSlot(&work->rig.anim, slotIndex, work->requestedSet);
    }
    work->playingSet = work->requestedSet;
}

/// Advances slots 1 to 5 and applies their poses at the combined driver rate.
///
/// `task->work` must expose a live `AnimDriverWork` prefix with initialized
/// slots and the borrowed bindings required by `animationTickSlot`.
/// `rate + rateBias` is narrowed to each slot's signed eight-bit rate, in
/// sixteenths of a frame per tick. Slot 0 and driver counters are unchanged.
static __inline__ void _animDriverTickSlots(Task* task)
{
    AnimDriverWork* work;
    s32             slotIndex;

    work = task->work;
    for (slotIndex = ANIM_DRIVER_FIRST_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->rate + work->rateBias;
        animationTickSlot(&work->rig.anim, slotIndex);
    }
}

/// Restarts slots 1 to 5 on the requested set and begins a new counter interval.
///
/// `work` must be a live writable `AnimDriverWork` prefix whose rig is bound
/// to its own slots and pose buffer and a live model with coordinates 1 to 5.
/// `requestedSet` must be a nonnegative loaded set-table index, excluding
/// `ANIMATION_SET_BUFFERED_POSE`, with valid track starts for all five indices.
/// The borrowed set table and clip data must remain live during playback;
/// set, track and record bounds are not checked.
///
/// Records `requestedSet` as `playingSet`, enters `ANIM_DRIVER_STATE_PLAYING`
/// and clears both counters, even when the selected set is unchanged. Slots
/// finish at `ANIMATION_RATE_ONE`; the next playing call installs the combined
/// driver rate and applies the pose. Restart itself writes no pose, leaves
/// slot 0 untouched and preserves the driver's rate settings.
static __inline__ void _animDriverRestart(AnimDriverWork* work)
{
    _animDriverResetSlots(work);
    work->state     = ANIM_DRIVER_STATE_PLAYING;
    work->tickCount = 0;
    work->jumpCount = 0;
}

/// Processes an animation restart request or advances the actor's five driven slots.
///
/// `task->work` must expose the live `AnimDriverWork` prefix. Its rig must
/// borrow its own slots and pose buffer, a live model with coordinates 1 to 5,
/// and loaded sets containing those tracks; playback has the bounds and
/// lifetime requirements of `animationResetSlot` and `animationTickSlot`.
///
/// Either restart state selects `requestedSet`, clears both counters and
/// enters playing without advancing or writing a pose. A playing call counts
/// one tick even when a zero rate or a boundary holds the pose. It increments
/// `jumpCount` once if slot 1 followed any control jump that tick, including
/// a boundary jump. Both counters retain their low 16 bits on overflow.
/// Other states do nothing. Calls are independent: callers may restart and
/// advance with two calls in one frame. Slot 0 is never driven.
static void _animDriverTick(Task* task)
{
    AnimDriverWork* work;

    work = task->work;
    if (work->state == ANIM_DRIVER_STATE_RESTART_1) {
        _animDriverRestart(work);
    } else if (work->state == ANIM_DRIVER_STATE_RESTART_2) {
        _animDriverRestart(work);
    } else if (work->state == ANIM_DRIVER_STATE_PLAYING) {
        work->tickCount++;
        _animDriverTickSlots(task);
        if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            work->jumpCount++;
        }
    }
}
