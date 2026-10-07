/* Part of the library; see maggot_caterpillar.h. Inline helpers the fragments use. */

/// Restarts changed animation requests or advances the seven driven part slots.
///
/// The live rig, model and clip tables must cover slots 1..7; slot zero is
/// untouched. The blend table must cover the requested `animId`. A restart
/// resets `animFrame` and blends each slot from record zero; an unchanged
/// request increments the frame counter and ticks each slot once.
static inline void _maggotCaterpillarTickAnimInline(Task* task)
{
    MaggotCaterpillarWork* work;
    s32                    slotIndex;
    s32                    blendFrames;

    work = task->work;
    // A changed request captures the current pose for the new clip blend.
    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrame   = 0;
        blendFrames       = gMaggotCaterpillarAnimBlend[work->animId];
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}
