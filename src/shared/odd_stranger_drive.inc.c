/* Part of the Odd Stranger library; see odd_stranger.h. */

/// The actor's per-frame animation driver. A pending clip change in
/// `animRequest` either cross-fades every pose slot from the previous clip to the
/// requested one (1) or restarts them on it (2); a blend request in `blendRequest`
/// restarts the blend context on `blendAnimId`. The slots are then advanced,
/// blended while `blendActive` is set (cleared once the pose context reports its
/// end), the head yaw in `lookYaw` eases toward `lookYawTarget` by at most 0x100 a
/// frame and turns two joints of the model by it, and the animation event for
/// the current state and frame is played at the model's pan and depth.
void oddStrangerDrive(Task* arg0)
{
    OddStrangerWork* seekWork;
    OddStrangerWork* resetWork;
    OddStrangerWork* secondaryWork;
    OddStrangerWork* tickWork;
    OddStrangerWork* work;
    Enemy*           enemy;
    s32              animation;
    s32              index;
    u32              table;
    s16              state;
    s32              seekIndex;
    s32              resetIndex;
    s32              secondaryIndex;
    s32              tickIndex;
    s32              seekSlotIndex;
    s32              resetSlotIndex;
    s32              secondarySlotIndex;
    s32              tickSlotIndex;
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

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    state = work->animRequest;
    if (state == ODD_STRANGER_ANIM_REQUEST_BLEND) {
        // Keep the copy before the comparison so it fills the branch delay slot.
        seekWork = arg0->work;
        if (work->appliedAnim != (s16)work->animId) {
            seekIndex = 1;
            table     = (u32)&gOddStrangerTransitions;
            do {
                seekSlotIndex                   = seekIndex;
                work->rig.slots[seekIndex].rate = seekWork->animRate;
                animation                       = (s16)seekWork->animId;
                index                           = seekWork->appliedAnim * 0x2D;
                animationSeekSlotWithBlend(&seekWork->rig.anim, seekSlotIndex, (s16)(animation), 0,
                                           (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x13);
            seekWork->appliedAnim = (s16)seekWork->animId;
        }
        work->animRequest  = ODD_STRANGER_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    } else if (state == ODD_STRANGER_ANIM_REQUEST_RESET) {
        resetWork = work;
        // Preserve the separate work pointer for the reset loop.
        resetIndex = 1;
        do {
            resetSlotIndex                   = resetIndex;
            work->rig.slots[resetIndex].rate = resetWork->animRate;
            animationResetSlot(&resetWork->rig.anim, resetSlotIndex,
                               (s16)resetWork->animId);
            resetIndex += 1;
        } while (resetIndex < 0x13);
        resetWork->appliedAnim = (s16)resetWork->animId;
        work->animRequest      = ODD_STRANGER_ANIM_REQUEST_PLAYING;
        work->animFrames       = 0;
        work->lastCueFrame     = 0;
    }
    if (work->blendRequest == ODD_STRANGER_ANIM_REQUEST_RESET) {
        secondaryWork              = arg0->work;
        secondaryIndex             = 1;
        secondaryWork->blendRate   = 0x30;
        secondaryWork->blendWeight = 0x800;
        do {
            secondarySlotIndex                            = secondaryIndex;
            secondaryWork->rig.slots[secondaryIndex].rate = secondaryWork->blendRate;
            animationResetSlot(&secondaryWork->blend.anim, secondarySlotIndex,
                               secondaryWork->blendAnimId);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x13);
        work->blendRequest = ODD_STRANGER_ANIM_REQUEST_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if ((s16)work->blendActive == 0) {
        tickWork  = arg0->work;
        tickIndex = 1;
        do {
            tickSlotIndex                       = tickIndex;
            tickWork->rig.slots[tickIndex].rate = tickWork->animRate;
            animationTickSlot(&tickWork->rig.anim, tickSlotIndex);
            tickIndex += 1;
        } while (tickIndex < 0x13);
    } else {
        oddStrangerTickBlended(arg0);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blendActive = 0;
        }
    }
    targetAngle      = (s16)work->lookYawTarget;
    currentAngle     = (s16)work->lookYaw;
    targetAngleBits  = (u16)work->lookYawTarget;
    currentAngleBits = (u16)work->lookYaw;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x101) {
            work->lookYaw = currentAngleBits + 0x100;
        } else {
            goto atTargetAngle;
        }
    } else if ((currentAngle - targetAngle) >= 0x101) {
        work->lookYaw = currentAngleBits - 0x100;
    } else {
    atTargetAngle:
        work->lookYaw = targetAngleBits;
    }
    angle        = (s16)work->lookYaw;
    clampedAngle = (u16)work->lookYaw;
    if (angle != 0) {
        if (angle >= 0x401) {
            clampedAngle = 0x400;
        }
        if (angle < -0x400) {
            clampedAngle = -0x400;
        }
        signedTurn = (s16)clampedAngle * 2 / 3;
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[5], signedTurn);
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[2], (s16)clampedAngle / 2);
        arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    sound = oddStrangerAnimEvent(work);
    if (sound != 0) {
        soundId = sound | (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, (s32)pan,
                            (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}
