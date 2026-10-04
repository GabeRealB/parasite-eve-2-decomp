/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Tick of the animation slots while the blend context is live: slots 1..10
/// sample both contexts and write their pose mixed by `blendWeight` (the blend
/// context gets the 0x1000 complement), each rate seeded from `animRate`
/// (three below it) and `blendRate`; slots 11..17 only tick the main context.
void desertChaserBlendTick(Task* task)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    DesertChaserWork* work;

    work   = task->work;
    weight = work->blendWeight;
    anim   = &work->rig.anim;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        if (i < 0xB) {
            work->blend.slots[i].rate = work->blendRate;
            work->rig.slots[i].rate   = (work->animRate - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blend.anim, i, &blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->rig.slots[i].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, i);
        }
    }
}
