/* Part of the Glutton library; see glutton.h. */

/// Reseed every slot of the three driving rigs from `animId` when
/// the id it names differs from the latched `appliedAnimId`, then latch it. Each
/// slot also has its `rate` seeded from `animRate`, and the reset argument
/// comes from the `[appliedAnimId][animId]` transition table.
void gluttonSwitchAnim(Task* arg0)
{
    GluttonWork* work = arg0->work;
    s32          i;

    if (work->appliedAnimId != work->animId) {
        for (i = 1; i < 8; i++) {
            work->hostRig.slots[i].rate = work->animRate;
            animationSeekSlotWithBlend(&work->hostRig.anim, i, work->animId, 0,
                                       gGluttonAnimTransitions[work->appliedAnimId][work->animId]);
        }
        for (i = 0; i < 4; i++) {
            work->escort0Rig.slots[i].rate = work->animRate;
            animationSeekSlotWithBlend(&work->escort0Rig.anim, i, work->animId, 0,
                                       gGluttonAnimTransitions[work->appliedAnimId][work->animId]);
        }
        for (i = 0; i < 4; i++) {
            work->escort1Rig.slots[i].rate = work->animRate;
            animationSeekSlotWithBlend(&work->escort1Rig.anim, i, work->animId, 0,
                                       gGluttonAnimTransitions[work->appliedAnimId][work->animId]);
        }
        work->appliedAnimId = work->animId;
    }
}
