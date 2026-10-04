/* Part of the library; see skull_stalker.h. Inline helpers the fragments use. */

/// Rebinds the second enemy's animation id `animId` to its two helper
/// slots: a changed id is remembered in `appliedAnim`, its frame count restarts
/// and both slots switch to it with a blend of 8; otherwise the count ticks and
/// the slots advance.
static __inline__ void skullStalkerTickAnim(Task* task)
{
    SkullStalkerWork* work = task->work;
    s32               i;

    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrames  = 0;
        for (i = 1; i < 3; i++) {
            animationSeekSlotWithBlend(&work->anim, i, work->animId, 0, 8);
        }
    } else {
        work->animFrames++;
        for (i = 1; i < 3; i++) {
            animationTickSlot(&work->anim, i);
        }
    }
}
