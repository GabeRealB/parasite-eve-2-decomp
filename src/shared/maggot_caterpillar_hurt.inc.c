#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// `MAGGOT_CATERPILLAR_BEHAVIOUR_HURT`, entered when a hit does damage. On
/// entry it starts `MAGGOT_CATERPILLAR_ANIM_HURT`, stops the forward and turn
/// steps and plays sound 0x401A0004 with the top nibble of the context's
/// `field_8` in bits 8-11, panned to the actor. Once the animation has run 0x15
/// frames it goes to `MAGGOT_CATERPILLAR_BEHAVIOUR_STUN` when `stunned` is 1,
/// otherwise to `MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM` with
/// `MAGGOT_CATERPILLAR_ANIM_IDLE` and a random 0..15 in `stateCounter`.
void maggotCaterpillarHurtState(Task* arg0)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    sound;
    s32                    pan;
    u32                    random;

    work  = arg0->work;
    state = work->step;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->animId       = MAGGOT_CATERPILLAR_ANIM_HURT;
            work->appliedAnim  = 1;
            work->step         = 1;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            sound              = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0004;
            pan                = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case 1:
            if (work->animFrame >= 0x15) {
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
