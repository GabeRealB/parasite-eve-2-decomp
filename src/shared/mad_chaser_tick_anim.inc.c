/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs the animation request in `animRequest` on animation slots 1..8: kind 1
/// blends into animation `animId` over `animBlendFrames` frames, kind 2 resets
/// the slots straight to it, and either records it as applied and moves to
/// kind 3, which counts frames in `animFrames`. Every frame each slot then
/// ticks at speed `animRate`.
void madChaserTickAnim(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* start;
    s32            i;
    s32            j;
    s32            k;

    work = (MadChaserWork*)arg0->work;
    if (work->animRequest == MAD_CHASER_ANIM_REQUEST_BLEND) {
        start = work;
        if (start->appliedAnim == start->animId) {
            for (i = 1; i < 9; i++) {
                (start->slots)[i].rate = start->animRate;
                animationSeekSlotWithBlend(&start->anim, i, start->animId, 0, start->animBlendFrames);
            }
        } else {
            for (i = 1; i < 9; i++) {
                (start->slots)[i].rate = start->animRate;
                animationSeekSlotWithBlend(&start->anim, i, start->animId, 0, start->animBlendFrames);
            }
            start->animBlendFrames = 0;
        }
        start->appliedAnim = start->animId;
        work->animRequest  = MAD_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
    } else if (work->animRequest == MAD_CHASER_ANIM_REQUEST_RESET) {
        start = work;
        for (j = 1; j < 9; j++) {
            animationResetSlot(&start->anim, j, start->animId);
            (start->slots)[j].rate = start->animRate;
        }
        start->appliedAnim = start->animId;
        work->animRequest  = MAD_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
    } else if (work->animRequest == MAD_CHASER_ANIM_REQUEST_PLAYING) {
        work->animFrames++;
    }
    for (k = 1; k < 9; k++) {
        (work->slots)[k].rate = work->animRate;
        animationTickSlot(&work->anim, k);
    }
}
