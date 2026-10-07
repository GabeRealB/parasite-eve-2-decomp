#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// `MAGGOT_CATERPILLAR_BEHAVIOUR_STUN`. On entry it starts
/// `MAGGOT_CATERPILLAR_ANIM_STUN` and stops the forward and turn steps; each
/// frame after that `damageTickEnemyBuildup` is ticked on the context, and when it
/// returns non-zero the actor goes to `MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM` with
/// `MAGGOT_CATERPILLAR_ANIM_HURT`, `stunned` cleared and a random 0..15 in
/// `stateCounter`.
void maggotCaterpillarStunState(Task* arg0)
{
    MaggotCaterpillarWork* work;
    s16                    state;
    u32                    random;

    work  = arg0->work;
    state = work->step;
    switch (state) {
        case 0:
            work->animId       = MAGGOT_CATERPILLAR_ANIM_STUN;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            work->step         = 1;
            return;
        case 1:
            if (damageTickEnemyBuildup(arg0->spawnArg2.pointer) != 0) {
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
