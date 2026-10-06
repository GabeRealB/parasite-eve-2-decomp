/* Part of the footstep walk library; see footstep_walk.h. */

/// Queues one spatial footstep when slot 1 enters a record with either foot cue.
///
/// `task->work` must hold the live sound walker's bound rig. Slot 1's borrowed
/// clip data and its latched record pointer stay valid while the rig uses them.
/// A buffered pose has no record; repeated records, no cue and both cue bits
/// together sound nothing. Every new record is latched, including uncued ones.
/// The model needs composed coordinate 1 for pan and attenuation and the
/// scratch/GTE state required by `worldCoordGetOriginAudioPan`. Queue failure
/// is ignored; the same held record is not retried. Pan and attenuation are
/// narrowed to signed bytes before being passed as argument words.
static void _footstepWalkPlayStepSound(Task* task)
{
    enum {
        FOOTSTEP_WALK_SOUND_CUE_1_ENTRY = 16,
        FOOTSTEP_WALK_SOUND_CUE_2_ENTRY = 15,
        FOOTSTEP_WALK_SOUND_ENTRY_BASE  = 100
    };

    FootstepWalkWork*      work;
    GfxCoord*              soundCoord;
    const AnimationRecord* record;
    s32                    cueBits;
    s32                    soundRequest;
    s32                    panOffset;
    s8                     attenuation;

    work       = task->work;
    soundCoord = task->extra.tmd->coords + 1;
    record     = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
    if (record == NULL || record == work->stepRecord) {
        return;
    }
    work->stepRecord = record;
    cueBits          = record->flags & ANIMATION_RECORD_CUE_MASK;
    if (cueBits != ANIMATION_RECORD_CUE_1 && cueBits != ANIMATION_RECORD_CUE_2) {
        return;
    }
    // The loaded type-1 bank's footstep entries are 115 and 116.
    soundRequest = SOUND_SCRIPT_REQUEST_TYPE_1 | FOOTSTEP_WALK_SOUND_CUE_2_ENTRY;
    if (cueBits == ANIMATION_RECORD_CUE_1) {
        soundRequest = SOUND_SCRIPT_REQUEST_TYPE_1 | FOOTSTEP_WALK_SOUND_CUE_1_ENTRY;
    }
    soundRequest += FOOTSTEP_WALK_SOUND_ENTRY_BASE;
    // Keep the pan's signed-byte conversion before querying attenuation.
    panOffset   = (s8)worldCoordGetOriginAudioPan(soundCoord);
    attenuation = worldCoordGetOriginAudioDepth(soundCoord);
    sndEvtRequestScriptStart(soundRequest, panOffset, attenuation);
}
