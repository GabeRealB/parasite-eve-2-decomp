/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests the light-recoil clip with an eight-frame normal-rate blend.
///
/// Requires initialized task-owned animation storage. Reloads live request
/// work; the frame callback applies the request and ticks the animation.
static __inline__ void _madChaserRecoilLightRequestClip(Task* task, s16 recoilClip)
{
    enum { MAD_CHASER_LIGHT_RECOIL_BLEND_FRAMES = 8 };
    MadChaserWork* requestWork = task->work;

    requestWork->animBlendFrames = MAD_CHASER_LIGHT_RECOIL_BLEND_FRAMES;
    requestWork->animRate        = ANIMATION_RATE_ONE;
    requestWork->animId          = recoilClip;
    requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
}

/// Starts light recoil and remembers the interrupted clip's stance for recovery.
///
/// Requires live enemy/model/work storage, initialized animation and a current
/// clip in 1..19. Upright stance blends to clip 11 and stops the instance-zero
/// alert cry while retaining voice release; other stances blend to clip 17.
/// Plays positional character-bank entry 3 at the root's already-composed origin
/// and advances subState to recovery. The blend lasts eight normal-rate frames;
/// playback belongs to the combat frame callback. Origin-audio scratch/projection
/// requirements apply, and all task/model storage stays live through the call.
static void _madChaserRecoilLight(Task* task)
{
    enum {
        MAD_CHASER_LIGHT_RECOIL_LOW_CLIP             = 17,
        MAD_CHASER_LIGHT_RECOIL_SOUND                = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 3),
        MAD_CHASER_LIGHT_RECOIL_SOUND_INSTANCE_SHIFT = 8,
    };
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;

    work               = task->work;
    work->stateScratch = gMadChaserAnimStance[work->animId - 1];
    if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
        _madChaserRecoilLightRequestClip(task, MAD_CHASER_LIGHT_RECOIL_UPRIGHT_CLIP);
        sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    } else {
        _madChaserRecoilLightRequestClip(task, MAD_CHASER_LIGHT_RECOIL_LOW_CLIP);
    }
    soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_LIGHT_RECOIL_SOUND_INSTANCE_SHIFT) | MAD_CHASER_LIGHT_RECOIL_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    work->subState++;
}
