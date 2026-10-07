/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Returns and latches the sound cue reached by the primary animation, or zero.
///
/// Uses slot 1's low ten record-index bits; these are cue indices, not elapsed
/// frames. Repeated sampling of a held cue returns zero. Clips with two cues
/// clear the latch between them; clips with no sound leave it untouched.
/// The caller adds the enemy's sound instance and supplies pan and depth.
static s32 _oddStrangerTakeAnimationSound(OddStrangerWork* work)
{
    s32 cueIndex;
    s32 latchedCueIndex;

    /// Takes a newly reached paired sound cue, rearming the latch between cues.
    ///
    /// Captures the stable work pointer and the cueIndex/latchedCueIndex locals.
    /// Cue arguments are tested once each; a sound argument is evaluated only
    /// when returned from this function. Pass side-effect-free integer values.
#define ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(firstCue, firstSound, secondCue, secondSound)              \
    {                                                                                                  \
        cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK; \
        if (cueIndex == (firstCue)) {                                                                  \
            if (work->lastCueFrame != cueIndex) {                                                      \
                work->lastCueFrame = cueIndex;                                                         \
                return (firstSound);                                                                   \
            }                                                                                          \
            work->lastCueFrame = cueIndex;                                                             \
        } else if (cueIndex == (secondCue)) {                                                          \
            latchedCueIndex = work->lastCueFrame;                                                      \
            if (latchedCueIndex != cueIndex) {                                                         \
                work->lastCueFrame = cueIndex;                                                         \
                return (secondSound);                                                                  \
            }                                                                                          \
            work->lastCueFrame = latchedCueIndex;                                                      \
        } else {                                                                                       \
            work->lastCueFrame = 0;                                                                    \
        }                                                                                              \
    }

    switch (work->animId) {
        case ODD_STRANGER_ANIM_SIDESTEP_NEGATIVE:
        case ODD_STRANGER_ANIM_SIDESTEP_POSITIVE:
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(7, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x10), 0x10, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x11));
            break;
        case ODD_STRANGER_ANIM_RUN:
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(0x1A, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x04), 0x13, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x03));
            break;
        case ODD_STRANGER_ANIM_WALK:
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(ODD_STRANGER_CLIP2_STEP_A, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x02), ODD_STRANGER_CLIP2_STEP_B, SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x01));
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
