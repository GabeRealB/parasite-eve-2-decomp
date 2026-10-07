/* Part of the Glutton library; see glutton.h. */

/// Starts the secondary clip used to blend the host and its first two escorts.
///
/// Requires initialized driving and secondary rigs in the host's work and a valid
/// `blendAnimId` in all three animation tables. Sets the blend rate to three
/// normal frames per tick and the driving-pose weight to one half. Host slots
/// 1..7 and escort slots 1..3 have their driving rate set and secondary clip
/// restarted; escort slot 0 is deliberately left alone.
static void _gluttonSeedBlend(Task* task)
{
    GluttonWork* work;
    s32          slotIndex;

    work              = task->work;
    work->blendRate   = 3 * ANIMATION_RATE_ONE;
    work->blendWeight = ONE / 2;
    slotIndex         = 1;
    do {
        work->hostRig.slots[slotIndex].rate = work->blendRate;
        animationResetSlot(&work->hostBlendRig.anim, slotIndex, work->blendAnimId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->hostRig.slots));
    slotIndex = 1;
    do {
        work->escort0Rig.slots[slotIndex].rate = work->blendRate;
        animationResetSlot(&work->escort0BlendRig.anim, slotIndex, work->blendAnimId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->escort0Rig.slots));
    slotIndex = 1;
    do {
        work->escort1Rig.slots[slotIndex].rate = work->blendRate;
        animationResetSlot(&work->escort1BlendRig.anim, slotIndex, work->blendAnimId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->escort1Rig.slots));
}
