/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Rebinds the Sucklerceph's animation id to its two helper slots unless
/// `animFrozen` suppresses it: a changed id is remembered, its frame count
/// restarts and both slots switch to it; otherwise the count ticks and the
/// slots advance.
static __inline__ void sucklercephTickAnim(Task* task)
{
    SucklercephWork* work = task->work;
    s32              i;
    if (work->animFrozen == 0) {
        if (work->animId != work->appliedAnim) {
            work->appliedAnim = work->animId;
            work->animFrames  = 0;
            for (i = 1; i < ARRAY_SIZE(work->slots); i++) {
                animationSeekSlotWithBlend(&work->anim, i, work->animId, 0, 0);
            }
        } else {
            work->animFrames++;
            for (i = 1; i < ARRAY_SIZE(work->slots); i++) {
                animationTickSlot(&work->anim, i);
            }
        }
    }
}
