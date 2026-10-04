/* Part of the Glutton library; see glutton.h. */

/// Seed the blend: `blendRate` to 0x30 and `blendWeight` to 0x800. Slots 1 and up
/// of each driving rig take that rate, and the same slots of its blend rig are
/// reset to animation `blendAnimId`.
void gluttonSeedBlend(Task* task)
{
    GluttonWork* work;
    s32          i;

    work              = task->work;
    work->blendRate   = 0x30;
    work->blendWeight = 0x800;
    i                 = 1;
    do {
        work->hostRig.slots[i].rate = work->blendRate;
        animationResetSlot(&work->hostBlendRig.anim, i, work->blendAnimId);
        i++;
    } while (i < 8);
    i = 1;
    do {
        work->escort0Rig.slots[i].rate = work->blendRate;
        animationResetSlot(&work->escort0BlendRig.anim, i, work->blendAnimId);
        i++;
    } while (i < 4);
    i = 1;
    do {
        work->escort1Rig.slots[i].rate = work->blendRate;
        animationResetSlot(&work->escort1BlendRig.anim, i, work->blendAnimId);
        i++;
    } while (i < 4);
}
