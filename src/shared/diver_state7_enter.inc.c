/* Part of the Diver library; see diver.h. */

/// Starts the recoil animation and voice, then advances to its waiting step.
///
/// Requires the live carrier work, initialized rig, loaded recoil clip and the
/// Enemy stored in `spawnArg2.pointer`. Queues a six-frame normal-rate blend;
/// the animation driver applies it. The placement index tags the sound instance,
/// with pan and attenuation sampled at the model root. Does not change the
/// carrier's state or clear hit flags; the next substate waits on clip status.
static void _diverEnterRecoil(Task* task)
{
    enum {
        DIVER_RECOIL_CLIP          = 10,
        DIVER_RECOIL_BLEND_FRAMES  = 6,
        DIVER_RECOIL_SOUND         = 0x40040006,
        DIVER_SOUND_INSTANCE_SHIFT = 8
    };
    s32        soundId;
    s32        panOffset;
    DiverWork* work;

    work = task->work;
    _diverRequestClipBlend(work, DIVER_RECOIL_CLIP, ANIMATION_RATE_ONE, DIVER_RECOIL_BLEND_FRAMES);
    soundId   = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << DIVER_SOUND_INSTANCE_SHIFT) | DIVER_RECOIL_SOUND;
    panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    work->subState++;
}
