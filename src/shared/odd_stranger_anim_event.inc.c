/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Returns and latches the sound cue reached by the primary animation, or zero.
///
/// Uses slot 1's low ten record-index bits; these are cue indices, not elapsed
/// frames. Repeated sampling of a held cue returns zero. Clips with two cues
/// clear the latch between them; clips with no sound leave it untouched.
/// The caller adds the enemy's sound instance and supplies pan and depth.
static s32 _oddStrangerTakeAnimationSound(OddStrangerWork* work)
{
    enum {
        ODD_STRANGER_SIDESTEP_FIRST_SOUND_CUE  = 7,
        ODD_STRANGER_SIDESTEP_SECOND_SOUND_CUE = 16,
        ODD_STRANGER_RUN_FIRST_SOUND_CUE       = 26,
        ODD_STRANGER_RUN_SECOND_SOUND_CUE      = 19
    };
    s32 cueIndex;
    s32 latchedCueIndex;

    /// Takes a newly reached paired sound cue, rearming the latch between cues.
    ///
    /// actorWork is a live OddStrangerWork pointer; cueIndex/latchedCueIndex are
    /// distinct writable s32 locals. These arguments are evaluated repeatedly
    /// and must be stable and side-effect-free. Captures no enclosing locals.
    /// Cue indices are distinct integers in 1..1023; firstCue is tested once,
    /// secondCue only if it differs. A selected, newly latched cue returns its
    /// sound from the enclosing s32 function; otherwise this block falls through.
    /// Other indices clear the latch to zero. Sound expressions are evaluated
    /// only on their return paths. Invoke as a standalone braced statement.
#define ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(actorWork, cueIndex, latchedCueIndex, firstCue, firstSound, secondCue, secondSound) \
    {                                                                                                                           \
        (cueIndex) = (actorWork)->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;                 \
        if ((cueIndex) == (firstCue)) {                                                                                         \
            if ((actorWork)->lastCueFrame != (cueIndex)) {                                                                      \
                (actorWork)->lastCueFrame = (cueIndex);                                                                         \
                return (firstSound);                                                                                            \
            }                                                                                                                   \
            (actorWork)->lastCueFrame = (cueIndex);                                                                             \
        } else if ((cueIndex) == (secondCue)) {                                                                                 \
            (latchedCueIndex) = (actorWork)->lastCueFrame;                                                                      \
            if ((latchedCueIndex) != (cueIndex)) {                                                                              \
                (actorWork)->lastCueFrame = (cueIndex);                                                                         \
                return (secondSound);                                                                                           \
            }                                                                                                                   \
            (actorWork)->lastCueFrame = (latchedCueIndex);                                                                      \
        } else {                                                                                                                \
            (actorWork)->lastCueFrame = 0;                                                                                      \
        }                                                                                                                       \
    }

    switch (work->animId) {
        case ODD_STRANGER_ANIM_SIDESTEP_NEGATIVE:
        case ODD_STRANGER_ANIM_SIDESTEP_POSITIVE:
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(work, cueIndex, latchedCueIndex, ODD_STRANGER_SIDESTEP_FIRST_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x10), ODD_STRANGER_SIDESTEP_SECOND_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x11));
            break;
        case ODD_STRANGER_ANIM_RUN:
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(work, cueIndex, latchedCueIndex, ODD_STRANGER_RUN_FIRST_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x04), ODD_STRANGER_RUN_SECOND_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x03));
            break;
        case ODD_STRANGER_ANIM_WALK:
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(work, cueIndex, latchedCueIndex, ODD_STRANGER_WALK_FIRST_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x02), ODD_STRANGER_WALK_SECOND_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x01));
            break;
        case ODD_STRANGER_ANIM_ALERT:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 4 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x06);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_DOWN_BACK:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 4 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x05);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_DOWN_FRONT:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 7 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x05);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_GRAB_REACH:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0xA) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x04);
                }
            }
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0x12) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x02);
                }
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_GRAB_PULL:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 9 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x0D);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_GRAB_RELEASE:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0x16 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x03);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_GRAB_STRIKE:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 9) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x0D);
                }
            }
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0x13) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x0C);
                }
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
    }
#undef ODD_STRANGER_TAKE_PAIRED_SOUND_CUES

    return 0;
}
