/* Part of the library; see skull_stalker.h. Inline helpers the fragments use. */

/// Rebinds animation requests or advances slots 1 and 2 of the model.
///
/// Requires initialized work and an animation id of idle or alert. A changed
/// request restarts its counter and seeks both slots to frame 0 with an
/// eight-tick blend; otherwise the counter and both slots advance. Slot 0 is
/// left alone. Animation ticks, rather than render frames, time fades and cries.
static __inline__ void _skullStalkerTickAnimation(Task* task)
{
    enum { SKULL_STALKER_ANIMATION_BLEND_TICKS = 8 };
    SkullStalkerWork* work = task->work;
    s32               slotIndex;

    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrames  = 0;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->anim, slotIndex, work->animId, 0, SKULL_STALKER_ANIMATION_BLEND_TICKS);
        }
    } else {
        work->animFrames++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
            animationTickSlot(&work->anim, slotIndex);
        }
    }
}
