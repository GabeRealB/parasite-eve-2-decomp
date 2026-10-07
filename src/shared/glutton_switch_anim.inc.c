/* Part of the Glutton library; see glutton.h. */

/// Blends the three driving rigs into the requested clip when it changes.
///
/// Requires initialized host and escort-0/1 rigs and both clip ids in the
/// transition table's domain (0..44); the requested clip must also exist in all
/// three rigs' animation tables. Host slots 1..7 and escort slots 0..3 seek the
/// requested clip's start at `animRate`, using the table's blend duration in
/// frames. The applied id is latched after all slots are reseeded. An unchanged
/// id leaves their playback and rates alone.
static void _gluttonSwitchAnim(Task* task)
{
    GluttonWork* work = task->work;
    s32          slotIndex;

    if (work->appliedAnimId != work->animId) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->hostRig.slots); slotIndex++) {
            work->hostRig.slots[slotIndex].rate = work->animRate;
            animationSeekSlotWithBlend(&work->hostRig.anim, slotIndex, work->animId, 0,
                                       gGluttonAnimTransitions[work->appliedAnimId][work->animId]);
        }
        for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->escort0Rig.slots); slotIndex++) {
            work->escort0Rig.slots[slotIndex].rate = work->animRate;
            animationSeekSlotWithBlend(&work->escort0Rig.anim, slotIndex, work->animId, 0,
                                       gGluttonAnimTransitions[work->appliedAnimId][work->animId]);
        }
        for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->escort1Rig.slots); slotIndex++) {
            work->escort1Rig.slots[slotIndex].rate = work->animRate;
            animationSeekSlotWithBlend(&work->escort1Rig.anim, slotIndex, work->animId, 0,
                                       gGluttonAnimTransitions[work->appliedAnimId][work->animId]);
        }
        work->appliedAnimId = work->animId;
    }
}
