/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Restarts the primary part tracks and records the requested animation as applied.
///
/// Requires a bound primary rig and a loaded set covering slots 1..18. Reset
/// installs normal rate; the driver reapplies the requested rate before ticking.
static __inline__ void _oddStrangerRestartPrimaryAnimation(OddStrangerWork* work)
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
                                           gOddStrangerTransitions[seekWork->appliedAnim][animationId]);
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
