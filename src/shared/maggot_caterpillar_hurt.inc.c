#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Plays a stationary hit reaction, then resumes stun or an idle roam phase.
///
/// The hurt clip is forced to restart even if it was already selected. Recovery
/// after 21 ticks retains pending stun; otherwise the idle lasts 0..15 ticks.
/// The placement index selects the spatial sound instance.
static void _maggotCaterpillarHurtState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_HURT_START   = 0,
        MAGGOT_CATERPILLAR_HURT_RECOVER = 1,
        MAGGOT_CATERPILLAR_HURT_FRAMES  = 21,
        MAGGOT_CATERPILLAR_HURT_SOUND   = 0x401A0004,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    step;
    s32                    soundKey;
    s32                    hurtPan;
    u32                    random;

    work  = actor->work;
    step  = work->step;
    coord = actor->extra.tmd->coords;
    switch (step) {
        case MAGGOT_CATERPILLAR_HURT_START:
            work->animId = MAGGOT_CATERPILLAR_ANIM_HURT;
            // Force the hurt request to differ even when the hurt clip repeats.
            work->appliedAnim  = MAGGOT_CATERPILLAR_ANIM_IDLE;
            work->step         = MAGGOT_CATERPILLAR_HURT_RECOVER;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            soundKey           = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_HURT_SOUND;
            hurtPan            = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(soundKey, hurtPan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case MAGGOT_CATERPILLAR_HURT_RECOVER:
            if (work->animFrame >= MAGGOT_CATERPILLAR_HURT_FRAMES) {
                if (work->stunned == 1) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_STUN;
                    work->step      = 0;
                    return;
                }
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState    = random;
                work->stateCounter = (random >> 0x10) & 0xF;
            } else {
                return;
            }
            break;
    }
}
