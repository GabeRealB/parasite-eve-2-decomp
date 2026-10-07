/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Selects the limp or resisting pull sequence from the current animation stance.
///
/// Requires initialized work, a live enemy/model and an animId with a valid
/// stance-table entry. Low stance blends clip 9 over four normal-rate frames,
/// plays the positional alert cry and selects limp drag (sub-state 4). Upright
/// stance blends clip 7 over eight frames, seeds the four-counts-per-rate-unit
/// ramp and advances to struggle. Animation playback belongs to the caller.
static void _madChaserPullReact(Task* task)
{
    enum {
        MAD_CHASER_PULL_LIMP_CLIP                 = 9,
        MAD_CHASER_PULL_LIMP_BLEND_FRAMES         = 4,
        MAD_CHASER_PULL_STRUGGLE_CLIP             = 7,
        MAD_CHASER_PULL_STRUGGLE_BLEND_FRAMES     = 8,
        MAD_CHASER_PULL_LIMP_SUB_STATE            = 4,
        MAD_CHASER_PULL_RATE_RAMP_COUNTS_PER_UNIT = 4,
    };
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;

    work = task->work;
    if (gMadChaserAnimStance[work->animId - 1] == MAD_CHASER_STANCE_LOW) {
        work->animBlendFrames = MAD_CHASER_PULL_LIMP_BLEND_FRAMES;
        work->animRate        = ANIMATION_RATE_ONE;
        work->animId          = MAD_CHASER_PULL_LIMP_CLIP;
        work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        soundId               = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_MAD_CHASER_ALERT_CRY;
        audioPan              = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->subState = MAD_CHASER_PULL_LIMP_SUB_STATE;
        return;
    }
    work->animBlendFrames = MAD_CHASER_PULL_STRUGGLE_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_PULL_STRUGGLE_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->stateScratch    = (u8)work->animRate * MAD_CHASER_PULL_RATE_RAMP_COUNTS_PER_UNIT;
    work->subState++;
}
