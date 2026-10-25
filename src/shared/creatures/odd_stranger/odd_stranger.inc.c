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
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(work, cueIndex, latchedCueIndex, ODD_STRANGER_SIDESTEP_FIRST_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x10), ODD_STRANGER_SIDESTEP_SECOND_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x11));
            break;
        case ODD_STRANGER_ANIM_RUN:
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(work, cueIndex, latchedCueIndex, ODD_STRANGER_RUN_FIRST_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x04), ODD_STRANGER_RUN_SECOND_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x03));
            break;
        case ODD_STRANGER_ANIM_WALK:
            ODD_STRANGER_TAKE_PAIRED_SOUND_CUES(work, cueIndex, latchedCueIndex, ODD_STRANGER_WALK_FIRST_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x02), ODD_STRANGER_WALK_SECOND_SOUND_CUE, SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x01));
            break;
        case ODD_STRANGER_ANIM_ALERT:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 4 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x06);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_DOWN_BACK:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 4 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x05);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_DOWN_FRONT:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 7 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x05);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_GRAB_REACH:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0xA) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x04);
                }
            }
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0x12) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x02);
                }
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_GRAB_PULL:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 9 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x0D);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_GRAB_RELEASE:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0x16 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x03);
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
        case ODD_STRANGER_ANIM_GRAB_STRIKE:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 9) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x0D);
                }
            }
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0x13) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x0C);
                }
            }
            latchedCueIndex    = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = latchedCueIndex;
            break;
    }
#undef ODD_STRANGER_TAKE_PAIRED_SOUND_CUES

    return 0;
}

/// Restarts the primary rig on its requested animation and records that selection.
///
/// `work->rig.anim` must be bound to `work->rig.slots` and nineteen live model
/// coordinates, with `animId` selecting a loaded set covering tracks 1..18.
/// Slot 0 remains the placed root. Reset installs normal rate; the driver
/// reapplies `animRate`, in sixteenths of a frame, before ticking the poses.
/// This records `appliedAnim` without consuming the request or clearing cues.
static inline void _oddStrangerRestartPrimaryAnimation(OddStrangerWork* work)
{
    s32 slotIndex;

    for (slotIndex = ODD_STRANGER_FIRST_ANIMATED_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationResetSlot(&work->rig.anim, slotIndex, work->animId);
    }
    work->appliedAnim = work->animId;
}

/// Applies animation requests, advances the parts, and updates look and sound.
///
/// Requires both rigs bound to live nineteen-part storage and a loaded bank.
/// Slot 0 is the placed root; slots 1..18 animate. A crossfade requires both
/// `appliedAnim` and `animId` to index the 45-by-45 transition table and the
/// requested set to be loaded. Rates use sixteenths of a frame per call;
/// `animFrames` counts calls modulo 65536. Applying a primary request clears
/// the counter and sound latch before advancing this call's poses.
/// The secondary rig mixes rotations of slots 1..10; translations stay primary.
/// Look angles use 4096 units per turn. Sound uses the enemy's place instance.
static void _oddStrangerDriveAnimation(Task* task)
{
    enum {
        ODD_STRANGER_LOOK_YAW_STEP  = 0x100, // 4096 units per turn
        ODD_STRANGER_LOOK_YAW_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 4
    };

    OddStrangerWork* seekWork;
    OddStrangerWork* resetWork;
    OddStrangerWork* secondaryWork;
    OddStrangerWork* tickWork;
    OddStrangerWork* work;
    Enemy*           enemy;
    s32              animationId;
    s16              request;
    s32              seekIndex;
    s32              secondaryIndex;
    s32              tickIndex;
    s32              targetAngle;
    s32              currentAngle;
    s32              targetAngleBits;
    s32              currentAngleBits;
    s16              angle;
    s32              clampedAngle;
    s16              signedTurn;
    s32              sound;
    s32              soundId;
    s32              pan;

    work    = task->work;
    enemy   = task->spawnArg2.pointer;
    request = work->animRequest;
    if (request == ODD_STRANGER_ANIM_REQUEST_BLEND) {
        // Keep the copy before the comparison so it fills the branch delay slot.
        seekWork = task->work;
        if (work->appliedAnim != work->animId) {
            for (seekIndex = ODD_STRANGER_FIRST_ANIMATED_SLOT; seekIndex < ARRAY_SIZE(seekWork->rig.slots); seekIndex++) {
                work->rig.slots[seekIndex].rate = seekWork->animRate;
                animationId                     = seekWork->animId;
                animationSeekSlotWithBlend(&seekWork->rig.anim, seekIndex, animationId, 0,
                                           _gOddStrangerTransitionFrames[seekWork->appliedAnim][animationId]);
            }
            seekWork->appliedAnim = seekWork->animId;
        }
        work->animRequest  = ODD_STRANGER_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    } else if (request == ODD_STRANGER_ANIM_REQUEST_RESET) {
        resetWork = work;
        _oddStrangerRestartPrimaryAnimation(resetWork);
        work->animRequest  = ODD_STRANGER_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    // Restart the secondary tracks, retaining the primary rig's rate stores.
    if (work->blendRequest == ODD_STRANGER_ANIM_REQUEST_RESET) {
        secondaryWork              = task->work;
        secondaryWork->blendRate   = 3 * ANIMATION_RATE_ONE;
        secondaryWork->blendWeight = ONE / 2;
        for (secondaryIndex = ODD_STRANGER_FIRST_ANIMATED_SLOT; secondaryIndex < ARRAY_SIZE(secondaryWork->blend.slots); secondaryIndex++) {
            secondaryWork->rig.slots[secondaryIndex].rate = secondaryWork->blendRate;
            animationResetSlot(&secondaryWork->blend.anim, secondaryIndex,
                               secondaryWork->blendAnimId);
        }
        work->blendRequest = ODD_STRANGER_ANIM_REQUEST_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->blendActive == 0) {
        tickWork = task->work;
        for (tickIndex = ODD_STRANGER_FIRST_ANIMATED_SLOT; tickIndex < ARRAY_SIZE(tickWork->rig.slots); tickIndex++) {
            tickWork->rig.slots[tickIndex].rate = tickWork->animRate;
            animationTickSlot(&tickWork->rig.anim, tickIndex);
        }
    } else {
        _oddStrangerTickBlendedAnimation(task);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blendActive = 0;
        }
    }
    // Ease the look, then distribute a clamped quarter-turn over the upper joints.
    targetAngle      = work->lookYawTarget;
    currentAngle     = work->lookYaw;
    targetAngleBits  = (u16)work->lookYawTarget;
    currentAngleBits = (u16)work->lookYaw;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) > ODD_STRANGER_LOOK_YAW_STEP) {
            work->lookYaw = currentAngleBits + ODD_STRANGER_LOOK_YAW_STEP;
        } else {
            work->lookYaw = targetAngleBits;
        }
    } else if ((currentAngle - targetAngle) > ODD_STRANGER_LOOK_YAW_STEP) {
        work->lookYaw = currentAngleBits - ODD_STRANGER_LOOK_YAW_STEP;
    } else {
        work->lookYaw = targetAngleBits;
    }
    angle        = work->lookYaw;
    clampedAngle = (u16)work->lookYaw;
    if (angle != 0) {
        if (angle > ODD_STRANGER_LOOK_YAW_LIMIT) {
            clampedAngle = ODD_STRANGER_LOOK_YAW_LIMIT;
        }
        if (angle < -ODD_STRANGER_LOOK_YAW_LIMIT) {
            clampedAngle = -ODD_STRANGER_LOOK_YAW_LIMIT;
        }
        signedTurn = (s16)clampedAngle * 2 / 3;
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[5], signedTurn);
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[2], (s16)clampedAngle / 2);
        task->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    sound = _oddStrangerTakeAnimationSound(work);
    if (sound != 0) {
        soundId = sound | (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan,
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}
