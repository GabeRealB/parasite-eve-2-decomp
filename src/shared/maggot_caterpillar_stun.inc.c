#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Holds the actor still until its buildup reaction expires.
///
/// Starts the stun clip once and ticks the enemy buildup timer on subsequent
/// calls. Expiry clears the stun latch and returns to roam with the hurt clip
/// and an idle delay of 0..15 ticks.
static void _maggotCaterpillarStunState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_STUN_START = 0,
        MAGGOT_CATERPILLAR_STUN_WAIT  = 1,
    };
    MaggotCaterpillarWork* work;
    s16                    step;
    u32                    random;

    work = actor->work;
    step = work->step;
    switch (step) {
        case MAGGOT_CATERPILLAR_STUN_START:
            work->animId       = MAGGOT_CATERPILLAR_ANIM_STUN;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            work->step         = MAGGOT_CATERPILLAR_STUN_WAIT;
            return;
        case MAGGOT_CATERPILLAR_STUN_WAIT:
            if (damageTickEnemyBuildup(actor->spawnArg2.pointer) != 0) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_HURT;
                work->stunned      = 0;
                random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState    = random;
                work->stateCounter = (random >> 0x10) & 0xF;
            }
            return;
    }
}
