/* Part of the library; see generator.h. Inline helpers the fragments use. */

/// Applies or ticks the body's animation-set request inside the death handler.
///
/// Requires initialized GeneratorWork; a changed request must be in 1..3.
/// Initial equal zero requests retain the idle slots started at spawn. A changed
/// request blends slots 1..9 from frame zero and resets animFrames; otherwise
/// those slots tick and the signed halfword counter advances. Slot 0 is unused.
/// The ordinary frame handler uses the same operation out of line.
static inline void _generatorUpdateAnimationInline(Task* task)
{
    GeneratorWork* work;
    s32            slotIndex;
    s32            blendFrames;

    work = task->work;
    if (work->animSet != work->appliedAnimSet) {
        work->appliedAnimSet = work->animSet;
        work->animFrames     = 0;
        blendFrames          = gGeneratorPoseStartFrames[work->animSet];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->anim, slotIndex, work->animSet, 0, blendFrames);
        }
    } else {
        work->animFrames++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
            animationTickSlot(&work->anim, slotIndex);
        }
    }
}
