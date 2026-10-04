#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sound 4 on the first frame; once the hit flags are set, draws a
/// 0x5A..0xD9 cooldown into `leapCooldown` and moves the state machine to
/// state 3.
void madChaserLeapLand(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s32            soundId;
    s32            pan;
    u32            rand;

    work       = (MadChaserWork*)arg0->work;
    work->busy = 0;
    if ((s16)++work->stateFrames == 1) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (madChaserAnimEnded(arg0) != 0) {
        rand               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState    = rand;
        work->leapCooldown = ((rand >> 16) & 0x7F) + 0x5A;
        work2              = (MadChaserWork*)arg0->work;
        work2->state       = 3;
        work2->subState    = 0;
    }
}
