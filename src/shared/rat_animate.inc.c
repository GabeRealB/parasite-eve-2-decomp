/* Part of the Rat library; see rat.h. */

/// When the requested animation differs from the playing one, restarts slots
/// 1-6 on it with the start value from gRatAnimBlend and resets the frame
/// counter; otherwise advances the frame counter and ticks the six slots.
void ratAnimate(Task* arg0)
{
    RatWork* work2;
    s32      i;
    s32      val;

    work2 = arg0->work;
    if (work2->animId != work2->appliedAnimId) {
        work2->appliedAnimId = work2->animId;
        work2->animFrame     = 0;
        val                  = gRatAnimBlend[work2->animId];
        for (i = 1; i < 7; i++) {
            animationSeekSlotWithBlend(&work2->rig.anim, i, work2->animId, 0, val);
        }
    } else {
        work2->animFrame++;
        for (i = 1; i < 7; i++) {
            animationTickSlot(&work2->rig.anim, i);
        }
    }
}
