#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts a walk: requests animation 7, plays the enemy's sound 1, draws a
/// random 0..0x7FF into `leapRangeBonus` and advances the sub-state. The animation
/// speed and the turn step `turnStep` grow with the distance to the nearer
/// player actor in `playerDist`, in bands of 1000.
void madChaserWalkStart(Task* arg0)
{
    MadChaserWork* work;
    s32            soundId;
    s32            pan;
    s16            step;

    work                  = (MadChaserWork*)arg0->work;
    work->animBlendFrames = 8;
    work->animId          = 7;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState++;
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0001;
    pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->leapRangeBonus = (gRandomLcgState >> 0x10) & 0x7FF;
    if (work->playerDist < 1000) {
        work->animRate = ANIMATION_RATE_ONE;
        work->turnStep = 0x10;
        return;
    }
    if (work->playerDist < 2000) {
        work->animRate = 0x14;
        step           = 0x12;
    } else if (work->playerDist < 3000) {
        work->animRate = 0x18;
        step           = 0x14;
    } else if (work->playerDist < 4000) {
        work->animRate = 0x1C;
        step           = 0x16;
    } else if (work->playerDist < 5000) {
        work->animRate = 0x20;
        step           = 0x18;
    } else {
        work->animRate = 0x40;
        step           = 0x20;
    }
    work->turnStep = step;
}
