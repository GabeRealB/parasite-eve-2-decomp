/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Per-frame animation tick. Seeds the slots when `field_828` asks for it
/// (1 from the per-state table `gRigClipStartFrames`, 2 by resetting to
/// `field_82E`), seeds the blend context when `field_836` is 2, then ticks
/// the slots - blended through `desertChaserBlendTick` while `field_82A`
/// is set. It eases `field_844` toward `field_840` and spreads it over the
/// body joints 2-4, eases `field_842` toward `field_83E` for joint 10, and
/// plays the sound `desertChaserAnimCues` returns, panned at the root.
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
    s32               pan;

    work  = (DesertChaserWork*)task->work;
    state = work->field_828;
    if (state == 1) {
        if (work->field_82C != work->field_82E) {
            seekWork  = work;
            seekIndex = 1;
            table     = (u32)gRigClipStartFrames;
            do {
                seekSlotIndex               = seekIndex;
                work->slots[seekIndex].rate = seekWork->field_832;
                animation                   = seekWork->field_82E;
                index                       = seekWork->field_82C * 0x2D;
                func_800B4114(&seekWork->anim, seekSlotIndex, (s16)(animation), 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->field_82C = seekWork->field_82E;
        }
        work->field_828 = 3;
        work->field_830 = 0;
        Mem_Set(work->field_848, 0U, 0x48U);
    } else if (state == 2) {
        resetWork  = work;
        resetIndex = 1;
        do {
            resetSlotIndex               = resetIndex;
            work->slots[resetIndex].rate = resetWork->field_832;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex, (s32)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(work->field_848, 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork            = (DesertChaserWork*)task->work;
        secondaryIndex           = 1;
        secondaryWork->field_83A = 0x20;
        secondaryWork->field_83C = 0x800;
        do {
            secondarySlotIndex                        = secondaryIndex;
            secondaryWork->slots[secondaryIndex].rate = secondaryWork->field_83A;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if (work->field_82A == 0) {
        tickWork  = (DesertChaserWork*)task->work;
        tickIndex = 1;
        do {
            tickSlotIndex                   = tickIndex;
            tickWork->slots[tickIndex].rate = tickWork->field_832;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        desertChaserBlendTick(task);
        if (work->blendSlots[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->field_82A = 0;
        }
    }
    targetAngle      = work->field_840;
    currentAngle     = work->field_844;
    targetAngleBits  = (u16)work->field_840;
    currentAngleBits = (u16)work->field_844;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_844 = currentAngleBits + 0x71;
        } else {
            goto snap;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_844 = currentAngleBits - 0x71;
    } else {
    snap:
        work->field_844 = targetAngleBits;
    }
    angle        = work->field_844;
    clampedAngle = (u16)work->field_844;
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
    turnWork     = (DesertChaserWork*)task->work;
    targetTurn   = (u16)turnWork->field_83E;
    originalTurn = targetTurn;
    if ((s16)targetTurn >= 0x201) {
        targetTurn = 0x200;
    }
    if ((s16)originalTurn < -0x200) {
        targetTurn = -0x200;
    }
    signedTurn  = (s16)targetTurn;
    currentTurn = turnWork->field_842;
    if (currentTurn < signedTurn) {
        if ((signedTurn - currentTurn) >= 0xD) {
            turnWork->field_842 = (s16)((u16)turnWork->field_842 + 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    updatedTurn     = turnWork->field_842;
    updatedTurnBits = (u16)turnWork->field_842;
    if ((s16)targetTurn < updatedTurn) {
        delta = updatedTurn - (s16)targetTurn;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 0xD) {
            turnWork->field_842 = (s16)(updatedTurnBits - 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    ActorContact_TurnJoint(&task->extra.tmd->coords[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    task->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    sound                                    = desertChaserAnimCues(task, work);
    if (sound != 0) {
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}
