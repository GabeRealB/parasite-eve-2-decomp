/* Part of the Rat library; see rat.h. */

/// Holds the rat down until its build-up reaction finishes.
///
/// Requires live work/model and an `Enemy` in `Task::spawnArg2.pointer`. A rat already knocked
/// down skips the collapse; both paths load a 0..15-frame count before holding.
/// Completion clears the build-up latch and requests an attack from idle.
static void _ratBuildup(Task* actor)
{
    enum {
        RAT_BUILDUP_COLLAPSE_END_FRAME = 29,
    };

    Enemy*   enemy;
    RatWork* work;
    s32      randomDelay;
    s32      randomDelayAfterCollapse;

    work = actor->work;
    switch (work->step) {
        case RAT_BUILDUP_STEP_BEGIN:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (work->knockedDown == 0) {
                work->step   = RAT_BUILDUP_STEP_COLLAPSE;
                work->animId = RAT_ANIM_COLLAPSE;
            } else {
                work->step      = RAT_BUILDUP_STEP_WAIT;
                randomDelay     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = randomDelay;
                work->timer     = ((u32)randomDelay >> 0x10) & 0xF;
            }
            work->knockedDown   = 1;
            work->appliedAnimId = RAT_ANIM_IDLE;
            return;
        case RAT_BUILDUP_STEP_COLLAPSE:
            if (work->animFrame >= RAT_BUILDUP_COLLAPSE_END_FRAME) {
                work->step               = RAT_BUILDUP_STEP_WAIT;
                randomDelayAfterCollapse = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState          = randomDelayAfterCollapse;
                work->timer              = ((u32)randomDelayAfterCollapse >> 0x10) & 0xF;
                return;
            }
            return;
        case RAT_BUILDUP_STEP_WAIT:
            work->timer--;
            if (work->timer <= 0) {
                work->animId = RAT_ANIM_BUILDUP_HOLD;
                work->step   = RAT_BUILDUP_STEP_HOLD;
                return;
            }
            break;
        case RAT_BUILDUP_STEP_HOLD:
            if (damageTickEnemyBuildup(actor->spawnArg2.pointer) != 0) {
                enemy                 = actor->spawnArg2.pointer;
                enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
                work->mode            = RAT_MODE_IDLE;
                work->step            = RAT_IDLE_STEP_REST;
                work->animId          = RAT_ANIM_IDLE;
                work->timer           = 0;
                work->attackRequested = 1;
                work->knockedDown     = 0;
                work->buildupHeld     = 0;
            }
            break;
    }
}
