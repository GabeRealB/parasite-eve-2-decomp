/* Part of the Odd Stranger library; see odd_stranger.h. */

/// The actor's per-frame animation driver. A pending clip change in
/// `field_898` either cross-fades every pose slot from the previous clip to the
/// requested one (1) or restarts them on it (2); a blend request in `field_8A6`
/// restarts the blend context on `field_8A8`. The slots are then advanced,
/// blended while `field_89A` is set (cleared once the pose context reports its
/// end), the head yaw in `field_8B0` eases toward `field_8AE` by at most 0x100 a
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

    work  = (OddStrangerWork*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    state = work->field_898;
    if (state == 1) {
        // Keep the copy before the comparison so it fills the branch delay slot.
        seekWork = (OddStrangerWork*)arg0->work;
        if (work->field_89C != (s16)work->field_89E) {
            seekIndex = 1;
            table     = (u32)&gOddStrangerTransitions;
            do {
                seekSlotIndex                   = seekIndex;
                work->rig.slots[seekIndex].rate = seekWork->field_8A2;
                animation                       = (s16)seekWork->field_89E;
                index                           = seekWork->field_89C * 0x2D;
                func_800B4114(&seekWork->rig.anim, seekSlotIndex, (s16)(animation), 0,
                              (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x13);
            seekWork->field_89C = (s16)seekWork->field_89E;
        }
        work->field_898 = 3;
        work->field_8A0 = 0;
        work->field_8B4 = 0;
    } else if (state == 2) {
        resetWork = work;
        // Preserve the separate work pointer for the reset loop.
        resetIndex = 1;
        do {
            resetSlotIndex                   = resetIndex;
            work->rig.slots[resetIndex].rate = resetWork->field_8A2;
            Gp_AnimResetSlot(&resetWork->rig.anim, resetSlotIndex,
                             (s32)(s16)resetWork->field_89E);
            resetIndex += 1;
        } while (resetIndex < 0x13);
        resetWork->field_89C = (s16)resetWork->field_89E;
        work->field_898      = 3;
        work->field_8A0      = 0;
        work->field_8B4      = 0;
    }
    if (work->field_8A6 == 2) {
        secondaryWork            = (OddStrangerWork*)arg0->work;
        secondaryIndex           = 1;
        secondaryWork->field_8AA = 0x30;
        secondaryWork->field_8AC = 0x800;
        do {
            secondarySlotIndex                            = secondaryIndex;
            secondaryWork->rig.slots[secondaryIndex].rate = secondaryWork->field_8AA;
            Gp_AnimResetSlot(&secondaryWork->blend.anim, secondarySlotIndex,
                             (s32)secondaryWork->field_8A8);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x13);
        work->field_8A6 = 3;
    }
    work->field_8A0 = (u16)(work->field_8A0 + 1);
    if ((s16)work->field_89A == 0) {
        tickWork  = (OddStrangerWork*)arg0->work;
        tickIndex = 1;
        do {
            tickSlotIndex                       = tickIndex;
            tickWork->rig.slots[tickIndex].rate = tickWork->field_8A2;
            Gp_AnimTickIndex(&tickWork->rig.anim, tickSlotIndex);
            tickIndex += 1;
        } while (tickIndex < 0x13);
    } else {
        oddStrangerTickBlended(arg0);
        if (work->blend.slots[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->field_89A = 0;
        }
    }
    targetAngle      = (s16)work->field_8AE;
    currentAngle     = (s16)work->field_8B0;
    targetAngleBits  = (u16)work->field_8AE;
    currentAngleBits = (u16)work->field_8B0;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x101) {
            work->field_8B0 = currentAngleBits + 0x100;
        } else {
            goto atTargetAngle;
        }
    } else if ((currentAngle - targetAngle) >= 0x101) {
        work->field_8B0 = currentAngleBits - 0x100;
    } else {
    atTargetAngle:
        work->field_8B0 = targetAngleBits;
    }
    angle        = (s16)work->field_8B0;
    clampedAngle = (u16)work->field_8B0;
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
