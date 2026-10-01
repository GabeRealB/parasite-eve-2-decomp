/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Tick of the animation slots while the blend context is live: slots 1..10
/// sample both contexts and write their pose mixed by `field_83C` (the blend
/// context gets the 0x1000 complement), each rate seeded from `field_832`
/// (three below it) and `field_83A`; slots 11..17 only tick the main context.
void desertChaserBlendTick(Task* task)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    DesertChaserWork* work;

    work   = (DesertChaserWork*)task->work;
    weight = work->field_83C;
    anim   = &work->anim;
    for (i = 1; i < 0x12; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = work->field_83A;
            work->slots[i].rate      = (work->field_832 - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blendAnim, i, &blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (work->field_832 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}
