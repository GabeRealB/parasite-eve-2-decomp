/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Eases a 4096-unit look bearing and distributes its clamped yaw over neck parts 2..4.
static __inline__ void _desertChaserApplyLookTurn(Task* task, DesertChaserWork* work)
{
    s32 targetAngle;
    s32 currentAngle;
    s32 targetAngleBits;
    s32 currentAngleBits;
    s16 angle;
    s32 clampedAngle;
    s16 thirdAngle;

    // Ease the look turn and distribute it over the three neck joints.
    targetAngle      = work->lookYawTarget;
    currentAngle     = work->lookYaw;
    targetAngleBits  = (u16)work->lookYawTarget;
    currentAngleBits = (u16)work->lookYaw;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->lookYaw = currentAngleBits + 0x71;
        } else {
            work->lookYaw = targetAngleBits;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->lookYaw = currentAngleBits - 0x71;
    } else {
        work->lookYaw = targetAngleBits;
    }
    angle        = work->lookYaw;
    clampedAngle = (u16)work->lookYaw;
    if (angle != 0) {
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[2], thirdAngle);
        task->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[3], thirdAngle);
        task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[4], (s16)clampedAngle / 2);
        task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Advances both animation rigs, applies look and waist turns, and emits animation cues.
///
/// Requires initialized work, model coordinates and this carrier's clip/cue tables.
/// Requested and previously applied clips must index the transition table (45
/// clips in the cutscene build, 25 in the armed builds). Slots 1..17 animate
/// parts; slot 0 stays untouched. Rates use sixteenths of a frame per tick and
/// turns use 4096 units per revolution. A call consumes pending reset/blend
/// requests and increments the wrapping animation tick counter.
static void _desertChaserAnimTick(Task* task)
{
    DesertChaserWork* work;
    DesertChaserWork* seekWork;
    DesertChaserWork* resetWork;
    DesertChaserWork* secondaryWork;
    DesertChaserWork* tickWork;
    DesertChaserWork* turnWork;
    s16               animRequest;
    s32               seekIndex;
    s32               seekSlotIndex;
    s32               animation;
    s32               resetIndex;
    s32               resetSlotIndex;
    s32               secondaryIndex;
    s32               secondarySlotIndex;
    s32               tickIndex;
    s32               tickSlotIndex;
    s32               targetTurn;
    u16               originalTurn;
    s32               signedTurn;
    s16               currentTurn;
    s32               updatedTurn;
    u16               updatedTurnBits;
    s32               delta;
    s32               sound;
#if DESERT_CHASER_CUE_NEEDS_PLACE
    s32 soundId;
#endif
    s32 pan;

    work        = task->work;
    animRequest = work->animRequest;
    if (animRequest == DESERT_CHASER_ANIM_REQUEST_BLEND) {
        if (work->appliedAnim != work->animId) {
            seekWork  = work;
            seekIndex = 1;
            do {
                seekSlotIndex                   = seekIndex;
                work->rig.slots[seekIndex].rate = seekWork->animRate;
                animation                       = seekWork->animId;
                animationSeekSlotWithBlend(&seekWork->rig.anim, seekSlotIndex, (s16)(animation), 0, gDesertChaserClipStartFrames[seekWork->appliedAnim][animation]);
                seekIndex += 1;
            } while (seekIndex < (s32)ARRAY_SIZE(work->rig.slots));
            seekWork->appliedAnim = seekWork->animId;
        }
        work->animRequest = DESERT_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
        memFillBytes(work->lastCueFrames, 0U, sizeof(work->lastCueFrames));
    } else if (animRequest == DESERT_CHASER_ANIM_REQUEST_RESET) {
        resetWork  = work;
        resetIndex = 1;
        do {
            resetSlotIndex                   = resetIndex;
            work->rig.slots[resetIndex].rate = resetWork->animRate;
            animationResetSlot(&resetWork->rig.anim, resetSlotIndex, resetWork->animId);
            resetIndex += 1;
        } while (resetIndex < (s32)ARRAY_SIZE(work->rig.slots));
        resetWork->appliedAnim = resetWork->animId;
        work->animRequest      = DESERT_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames       = 0U;
        memFillBytes(work->lastCueFrames, 0U, sizeof(work->lastCueFrames));
    }
    if (work->blendRequest == DESERT_CHASER_ANIM_REQUEST_RESET) {
        secondaryWork  = task->work;
        secondaryIndex = 1;
#if DESERT_CHASER_BLEND_RATE_RESET
        secondaryWork->blendRate   = 2 * ANIMATION_RATE_ONE;
        secondaryWork->blendWeight = DESERT_CHASER_BLEND_WEIGHT_HALF;
#endif
        // The restart writes the main rig rate; the following tick overwrites it.
        do {
            secondarySlotIndex                            = secondaryIndex;
            secondaryWork->rig.slots[secondaryIndex].rate = secondaryWork->blendRate;
            animationResetSlot(&secondaryWork->blend.anim, secondarySlotIndex, secondaryWork->blendAnimId);
            secondaryIndex += 1;
        } while (secondaryIndex < (s32)ARRAY_SIZE(secondaryWork->rig.slots));
        work->blendRequest = DESERT_CHASER_ANIM_REQUEST_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->blendActive == 0) {
        tickWork  = task->work;
        tickIndex = 1;
        do {
            tickSlotIndex                       = tickIndex;
            tickWork->rig.slots[tickIndex].rate = tickWork->animRate;
            animationTickSlot(&tickWork->rig.anim, tickSlotIndex);
            tickIndex += 1;
        } while (tickIndex < (s32)ARRAY_SIZE(tickWork->rig.slots));
    } else {
        _desertChaserBlendTick(task);
        if (work->blend.slots[1].status.fields.flags & DESERT_CHASER_BLEND_DONE) {
            work->blendActive = 0;
        }
    }
    _desertChaserApplyLookTurn(task, work);
#if DESERT_CHASER_STATE26_TILT
    if ((work->animId == 0) && (work->state == DESERT_CHASER_STATE_ROAM)) {
        gfxRotMatrixX(&task->extra.tmd->coords[4].coord, 0x280, GRAPHICS_ROTATION_COMPOSE);
        task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[4]);
    }
#endif
    // Clamp and ease the waist counter-turn independently of the neck.
    turnWork     = task->work;
    targetTurn   = (u16)turnWork->waistYawTarget;
    originalTurn = targetTurn;
    if ((s16)targetTurn >= 0x201) {
        targetTurn = 0x200;
    }
    if ((s16)originalTurn < -0x200) {
        targetTurn = -0x200;
    }
    signedTurn  = (s16)targetTurn;
    currentTurn = turnWork->waistYaw;
    if (currentTurn < signedTurn) {
        if ((signedTurn - currentTurn) >= 0xD) {
            turnWork->waistYaw = (s16)((u16)turnWork->waistYaw + 0xC);
        } else {
            turnWork->waistYaw = (s16)targetTurn;
        }
    }
    updatedTurn     = turnWork->waistYaw;
    updatedTurnBits = (u16)turnWork->waistYaw;
    if ((s16)targetTurn < updatedTurn) {
        delta = updatedTurn - (s16)targetTurn;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 0xD) {
            turnWork->waistYaw = (s16)(updatedTurnBits - 0xC);
        } else {
            turnWork->waistYaw = (s16)targetTurn;
        }
    }
    _actorRenderYawJointInWorld(&task->extra.tmd->coords[10], (s16)((s32)(u16)turnWork->waistYaw * -1));
    task->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    sound                                    = _desertChaserAnimCues(task, work);
    if (sound != 0) {
#if DESERT_CHASER_CUE_NEEDS_PLACE
        soundId = sound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
#else
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
#endif
    }
}
