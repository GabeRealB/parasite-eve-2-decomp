#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the combat approach with a distance-scaled walk and random leap range.
///
/// Requires live enemy/model/work storage, a tracked player distance and the
/// walk entry sub-state. Blends clip 7 over eight frames, advances to approach,
/// plays the positional walk cue and consumes one resident LCG draw for a
/// 0..2047-unit range bonus. Distance bands of 1000 parent-coordinate units
/// select rates 16/20/24/28/32/64 (sixteenths of a frame) and heading steps
/// 16/18/20/22/24/32 (4096ths of a turn). Animation ticking belongs to the caller.
static void _madChaserWalkStart(Task* task)
{
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;
    s16            turnStep;

    work                  = task->work;
    work->animBlendFrames = MAD_CHASER_WALK_BLEND_FRAMES;
    work->animId          = MAD_CHASER_WALK_CLIP;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState++;
    soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 1);
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->leapRangeBonus = (gRandomLcgState >> 16) & MAD_CHASER_WALK_LEAP_RANGE_BONUS_MASK;
    if (work->playerDist < MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE;
        work->turnStep = MAD_CHASER_WALK_BASE_TURN_STEP;
        return;
    }
    if (work->playerDist < 2 * MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE + MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep       = MAD_CHASER_WALK_BASE_TURN_STEP + MAD_CHASER_WALK_TURN_BAND_STEP;
    } else if (work->playerDist < 3 * MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE + 2 * MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep       = MAD_CHASER_WALK_BASE_TURN_STEP + 2 * MAD_CHASER_WALK_TURN_BAND_STEP;
    } else if (work->playerDist < 4 * MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE + 3 * MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep       = MAD_CHASER_WALK_BASE_TURN_STEP + 3 * MAD_CHASER_WALK_TURN_BAND_STEP;
    } else if (work->playerDist < 5 * MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE + 4 * MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep       = MAD_CHASER_WALK_BASE_TURN_STEP + 4 * MAD_CHASER_WALK_TURN_BAND_STEP;
    } else {
        work->animRate = MAD_CHASER_WALK_FAR_RATE;
        turnStep       = MAD_CHASER_WALK_FAR_TURN_STEP;
    }
    work->turnStep = turnStep;
}
