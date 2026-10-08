/* Part of the Rat library; see rat.h. */

/// Restarts or advances the rat's six non-root animation slots.
///
/// Requires live `RatWork` and animation/blend entries for the requested
/// `RAT_ANIM_*` value. A changed request starts slots 1..6 at frame zero with
/// the table's whole-frame blend duration; an unchanged request advances
/// the slots and the signed 16-bit elapsed-frame counter once.
static void _ratAnimate(Task* actor)
{
    RatWork* work;
    s32      slot;
    s32      blendFrames;

    work = actor->work;
    if (work->animId != work->appliedAnimId) {
        work->appliedAnimId = work->animId;
        work->animFrame     = 0;
        blendFrames         = gRatAnimBlend[work->animId];
        for (slot = 1; slot < ARRAY_SIZE(work->rig.slots); slot++) {
            animationSeekSlotWithBlend(&work->rig.anim, slot, work->animId, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slot = 1; slot < ARRAY_SIZE(work->rig.slots); slot++) {
            animationTickSlot(&work->rig.anim, slot);
        }
    }
}
