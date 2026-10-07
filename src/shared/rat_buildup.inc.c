/* Part of the Rat library; see rat.h. */

/// Behaviour mode 3: stops, plays animation 6 the first time (or waits a random
/// 0-15 frames if already latched), then plays animation 8 and waits until
/// damageTickEnemyBuildup reports the build-up effect over. It then clears the build-
/// up reaction flag and returns to mode 0 with the sensor flag latched.
void ratBuildup(Task* arg0)
{
    Enemy*   ctx;
    RatWork* work;
    s16      state;
    s32      rng;
    s32      rng2;

    work  = arg0->work;
    state = work->step;
    switch (state) {
        case 0:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (work->knockedDown == 0) {
                work->step   = 1;
                work->animId = RAT_ANIM_COLLAPSE;
            } else {
                work->step      = 2;
                rng             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                work->timer     = ((u32)rng >> 0x10) & 0xF;
            }
            work->knockedDown   = 1;
            work->appliedAnimId = RAT_ANIM_IDLE;
            return;
        case 1:
            if (work->animFrame >= 0x1D) {
                work->step      = 2;
                rng2            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng2;
                work->timer     = ((u32)rng2 >> 0x10) & 0xF;
                return;
            }
            return;
        case 2:
            work->timer--;
            if (work->timer <= 0) {
                work->animId = RAT_ANIM_BUILDUP_HOLD;
                work->step   = 3;
                return;
            }
            break;
        case 3:
            if (damageTickEnemyBuildup(arg0->spawnArg2.pointer) != 0) {
                ctx                   = arg0->spawnArg2.pointer;
                ctx->reactionFlags   &= ENEMY_REACTION_BUILDUP_CLEAR;
                work->mode            = RAT_MODE_IDLE;
                work->step            = 0;
                work->animId          = RAT_ANIM_IDLE;
                work->timer           = 0;
                work->attackRequested = 1;
                work->knockedDown     = 0;
                work->buildupHeld     = 0;
            }
            break;
    }
}
