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
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = gRatAnimBlend[(s16)work2->field_37E];
        for (i = 1; i < 7; i++) {
            func_800B4114(&work2->anim, i, (s16)work2->field_37E, 0, val);
        }
    } else {
        work2->field_382++;
        for (i = 1; i < 7; i++) {
            animationTickSlot(&work2->anim, i);
        }
    }
}
