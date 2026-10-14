/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Applies an animation request or advances the Sucklerceph's two moving parts.
///
/// Requires initialized `SucklercephWork` with three model parts, playback slots
/// and pose buffers and loaded idle/crawl sets. Slot 0 stays unstarted. A changed
/// request rebinds slots 1 and 2 at record 0 without a timed blend and clears
/// `animFrames`; an unchanged request increments that u16 counter and ticks both
/// slots. `animFrozen` holds the request, counter and poses. No pointer is retained.
static __inline__ void _sucklercephTickAnim(Task* task)
{
    SucklercephWork* work = task->work;
    s32              slotIndex;
    if (work->animFrozen == 0) {
        if (work->animId != work->appliedAnim) {
            work->appliedAnim = work->animId;
            work->animFrames  = 0;
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->anim, slotIndex, work->animId, 0, 0);
            }
        } else {
            work->animFrames++;
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
                animationTickSlot(&work->anim, slotIndex);
            }
        }
    }
}
