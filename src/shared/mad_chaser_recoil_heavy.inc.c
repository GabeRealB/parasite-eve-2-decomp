/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts heavy recoil with an animation selected by the interrupted stance.
///
/// Requires live work, enemy/model storage and an animId with a stance-table entry.
/// Saves that stance in stateScratch for recovery. Upright recoil blends clip 12
/// over two normal-rate frames and stops the global alert cry retaining release;
/// low recoil blends clip 17 over eight frames. Plays the positional hurt/death
/// cue and advances the sub-state; animation playback belongs to the caller.
static void _madChaserRecoilHeavy(Task* task)
{
    enum {
        MAD_CHASER_HEAVY_RECOIL_UPRIGHT_CLIP         = 12,
        MAD_CHASER_HEAVY_RECOIL_UPRIGHT_BLEND_FRAMES = 2,
        MAD_CHASER_HEAVY_RECOIL_LOW_CLIP             = 17,
        MAD_CHASER_HEAVY_RECOIL_LOW_BLEND_FRAMES     = 8,
        MAD_CHASER_HEAVY_RECOIL_SOUND                = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 3),
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            soundId;
    s32            audioPan;

    work               = task->work;
    work->stateScratch = gMadChaserAnimStance[work->animId - 1];
    if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_HEAVY_RECOIL_UPRIGHT_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_HEAVY_RECOIL_UPRIGHT_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    } else {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_HEAVY_RECOIL_LOW_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_HEAVY_RECOIL_LOW_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_HEAVY_RECOIL_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    work->subState++;
}
