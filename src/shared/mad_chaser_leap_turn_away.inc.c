#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sound 4 on the first frame and, once the hit flags are set, turns
/// the enemy around, draws a 0x5A..0xD9 cooldown into `leapCooldown`, requests
/// animation 0xD and moves the task to state 1.
void madChaserLeapTurnAway(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    MadChaserWork* work3;
    s32            soundId;
    s32            pan;
    u32            rand;

    work = (MadChaserWork*)arg0->work;
    if ((s16)++work->stateFrames == 1) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (madChaserAnimEnded(arg0) != 0) {
        work->busy         = 0;
        rand               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->leapCooldown = ((rand >> 16) & 0x7F) + 0x5A;
        work->rotation.vy += 0x800;
        work2              = (MadChaserWork*)arg0->work;
        work2->animRate    = ANIMATION_RATE_ONE;
        work2->animId      = 0xD;
        work2->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
        work3              = (MadChaserWork*)arg0->work;
        gRandomLcgState    = rand;
        arg0->state        = 1;
        work3->state       = 0;
        work3->subState    = 0;
    }
}
