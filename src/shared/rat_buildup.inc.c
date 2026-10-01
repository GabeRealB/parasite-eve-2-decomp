/* Part of the Rat library; see rat.h. */

/// Behaviour mode 3: stops, plays animation 6 the first time (or waits a random
/// 0-15 frames if already latched), then plays animation 8 and waits until
/// Gp_TickObjFlag2 reports the build-up effect over. It then clears the build-
/// up reaction flag and returns to mode 0 with the sensor flag latched.
void ratBuildup(Task* arg0)
{
    Enemy*   ctx;
    RatWork* work;
    s16      state;
    s32      rng;
    s32      rng2;
    u16      timer;

    work  = arg0->work;
    state = work->field_37C;
    switch (state) {
        case 0:
            work->field_384 = 0;
            work->field_386 = 0;
            if (work->field_396 == 0) {
                work->field_37C = 1;
                work->field_37E = 6;
            } else {
                work->field_37C = 2;
                rng             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                work->field_38C = ((u32)rng >> 0x10) & 0xF;
            }
            work->field_396 = 1;
            work->field_380 = 1;
            return;
        case 1:
            if ((s16)work->field_382 >= 0x1D) {
                work->field_37C = 2;
                rng2            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng2;
                work->field_38C = ((u32)rng2 >> 0x10) & 0xF;
                return;
            }
            return;
        case 2:
            timer           = work->field_38C - 1;
            work->field_38C = timer;
            if ((timer << 0x10) <= 0) {
                work->field_37E = 8;
                work->field_37C = 3;
                return;
            }
            break;
        case 3:
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                ctx                 = arg0->spawnArg2.pointer;
                ctx->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
                work->field_37A     = 0;
                work->field_37C     = 0;
                work->field_37E     = 1;
                work->field_38C     = 0;
                work->field_394     = 1;
                work->field_396     = 0;
                work->field_398     = 0;
            }
            break;
    }
}
