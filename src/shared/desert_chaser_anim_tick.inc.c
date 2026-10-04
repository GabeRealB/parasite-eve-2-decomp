/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Per-frame animation tick. Seeds the slots when `animRequest` asks for it
/// (`BLEND` with the length `gDesertChaserClipStartFrames` gives the
/// transition, `RESET` by restarting on `animId`), seeds the blend rig when
/// `blendRequest` is `RESET`, then ticks the slots - blended through
/// `desertChaserBlendTick` while `blendActive` is set. It eases `lookYaw`
/// toward `lookYawTarget` and spreads it over the neck parts 2-4, eases
/// `waistYaw` toward `waistYawTarget` and turns the waist, part 10, against
/// it, and plays the sound `desertChaserAnimCues` returns, panned at the root.
/// The blend tick and the cue step are each build's own. The armed builds
/// also tip part 4 forward in state 0x26 while no clip is queued, and the
/// regular build adds the placement index to the cue's sound id.
void desertChaserAnimTick(Task* task)
{
    DesertChaserWork* work;
    DesertChaserWork* seekWork;
    DesertChaserWork* resetWork;
    DesertChaserWork* secondaryWork;
    DesertChaserWork* tickWork;
    DesertChaserWork* turnWork;
    u32               table;
    s16               state;
    s32               seekIndex;
    s32               seekSlotIndex;
    s32               animation;
    s32               index;
    s32               resetIndex;
    s32               resetSlotIndex;
    s32               secondaryIndex;
    s32               secondarySlotIndex;
    s32               tickIndex;
    s32               tickSlotIndex;
    s32               targetAngle;
    s32               currentAngle;
    s32               targetAngleBits;
    s32               currentAngleBits;
    s16               angle;
    s32               clampedAngle;
    s16               thirdAngle;
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

    work  = task->work;
    state = work->animRequest;
    if (state == DESERT_CHASER_ANIM_REQUEST_BLEND) {
        if (work->appliedAnim != work->animId) {
            seekWork  = work;
            seekIndex = 1;
            table     = (u32)gDesertChaserClipStartFrames;
            do {
                seekSlotIndex                   = seekIndex;
                work->rig.slots[seekIndex].rate = seekWork->animRate;
                animation                       = seekWork->animId;
                index                           = seekWork->appliedAnim * DESERT_CHASER_CLIP_COUNT;
                animationSeekSlotWithBlend(&seekWork->rig.anim, seekSlotIndex, (s16)(animation), 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->appliedAnim = seekWork->animId;
        }
        work->animRequest = DESERT_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
        memFillBytes(work->lastCueFrames, 0U, sizeof(work->lastCueFrames));
    } else if (state == DESERT_CHASER_ANIM_REQUEST_RESET) {
        resetWork  = work;
        resetIndex = 1;
        do {
            resetSlotIndex                   = resetIndex;
            work->rig.slots[resetIndex].rate = resetWork->animRate;
            animationResetSlot(&resetWork->rig.anim, resetSlotIndex, resetWork->animId);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->appliedAnim = resetWork->animId;
        work->animRequest      = DESERT_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames       = 0U;
        memFillBytes(work->lastCueFrames, 0U, sizeof(work->lastCueFrames));
    }
    if (work->blendRequest == DESERT_CHASER_ANIM_REQUEST_RESET) {
        secondaryWork  = task->work;
        secondaryIndex = 1;
#if DESERT_CHASER_BLEND_RATE_RESET
        secondaryWork->blendRate   = 0x20;
        secondaryWork->blendWeight = 0x800;
#endif
        do {
            secondarySlotIndex                            = secondaryIndex;
            secondaryWork->rig.slots[secondaryIndex].rate = secondaryWork->blendRate;
            animationResetSlot(&secondaryWork->blend.anim, secondarySlotIndex, secondaryWork->blendAnimId);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
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
        } while (tickIndex < 0x12);
    } else {
        desertChaserBlendTick(task);
        if (work->blend.slots[1].status.fields.flags & DESERT_CHASER_BLEND_DONE) {
            work->blendActive = 0;
        }
    }
    targetAngle      = work->lookYawTarget;
    currentAngle     = work->lookYaw;
    targetAngleBits  = (u16)work->lookYawTarget;
    currentAngleBits = (u16)work->lookYaw;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->lookYaw = currentAngleBits + 0x71;
        } else {
            goto snap;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->lookYaw = currentAngleBits - 0x71;
    } else {
    snap:
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
        ActorContact_TurnJoint(&task->extra.tmd->coords[2], thirdAngle);
        task->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        ActorContact_TurnJoint(&task->extra.tmd->coords[3], thirdAngle);
        task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        ActorContact_TurnJoint(&task->extra.tmd->coords[4], (s16)clampedAngle / 2);
        task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    }
#if DESERT_CHASER_STATE26_TILT
    if ((work->animId == 0) && (work->state == 0x26)) {
        gfxRotMatrixX(&task->extra.tmd->coords[4].coord, 0x280, GRAPHICS_ROTATION_COMPOSE);
        task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&task->extra.tmd->coords[4]);
    }
#endif
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
    ActorContact_TurnJoint(&task->extra.tmd->coords[10], (s16)((s32)(u16)turnWork->waistYaw * -1));
    task->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    sound                                    = desertChaserAnimCues(task, work);
    if (sound != 0) {
#if DESERT_CHASER_CUE_NEEDS_PLACE
        soundId = sound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
#else
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
#endif
    }
}
