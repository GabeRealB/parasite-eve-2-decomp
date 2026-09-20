#include "common.h"

#include "actors/actors_shared_80133eb8.h"
#include "actors/actors_shared_80132808.h"
#include "gameplay/3A34.h"
#include "main/sound.h"

extern s8 ActorsShared80133eb8_Transitions;
void      func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);
void      ActorsShared80133eb8_BlendTick(ActorsShared80133eb8Actor* arg0);
s32       ActorsShared80133eb8_Sound(ActorsShared80133eb8Work* arg0);

/// Shared animation driver for actor_401000 and actor_401800.
void ActorsShared80133eb8(ActorsShared80133eb8Actor* arg0)
{
    ActorsShared80133eb8Work* seekWork;
    ActorsShared80133eb8Work* resetWork;
    ActorsShared80133eb8Work* secondaryWork;
    ActorsShared80133eb8Work* tickWork;
    ActorsShared80133eb8Work* work;
    GpEnemy*                  enemy;
    s32                       animation;
    s32                       index;
    u32                       table;
    s16                       state;
    s32                       seekIndex;
    s32                       resetIndex;
    s32                       secondaryIndex;
    s32                       tickIndex;
    s32                       seekSlotIndex;
    s32                       resetSlotIndex;
    s32                       secondarySlotIndex;
    s32                       tickSlotIndex;
    s32                       targetAngle;
    s32                       currentAngle;
    s32                       targetAngleBits;
    s32                       currentAngleBits;
    s16                       angle;
    s32                       clampedAngle;
    s16                       signedTurn;
    s32                       sound;
    s32                       soundId;
    s32                       pan;
    s8*                       seekSlot;
    s8*                       resetSlot;
    s8*                       secondarySlot;
    s8*                       tickSlot;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    state = work->field_898;
    if (state == 1) {
        // Keep the copy before the comparison so it fills the branch delay slot.
        seekWork = arg0->field_1C;
        if (work->field_89C != (s16)work->field_89E) {
            seekIndex = 1;
            table     = (u32)&ActorsShared80133eb8_Transitions;
            // Slot i has stride 0x28; its rate is at work + 0x39 + i * 0x28.
            seekSlot = ((s8*)work + 0x28);
            do {
                seekSlotIndex  = seekIndex;
                seekSlot[0x39] = (u8)seekWork->field_8A2;
                animation      = (s16)seekWork->field_89E;
                seekSlot      += 0x28;
                index          = seekWork->field_89C * 0x2D;
                func_800B4114(&seekWork->anim, seekSlotIndex, animation, 0,
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
        TOUCH_REG(resetWork);
        resetIndex = 1;
        resetSlot  = ((s8*)work + 0x28);
        do {
            resetSlotIndex  = resetIndex;
            resetSlot[0x39] = (u8)resetWork->field_8A2;
            resetSlot      += 0x28;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex,
                             (s32)(s16)resetWork->field_89E);
            resetIndex += 1;
        } while (resetIndex < 0x13);
        resetWork->field_89C = (s16)resetWork->field_89E;
        work->field_898      = 3;
        work->field_8A0      = 0;
        work->field_8B4      = 0;
    }
    if (work->field_8A6 == 2) {
        secondaryWork            = arg0->field_1C;
        secondaryIndex           = 1;
        secondarySlot            = ((s8*)secondaryWork + 0x28);
        secondaryWork->field_8AA = 0x30;
        secondaryWork->field_8AC = 0x800;
        do {
            secondarySlotIndex  = secondaryIndex;
            secondarySlot[0x39] = (u8)secondaryWork->field_8AA;
            secondarySlot      += 0x28;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex,
                             (s32)secondaryWork->field_8A8);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x13);
        work->field_8A6 = 3;
    }
    work->field_8A0 = (u16)(work->field_8A0 + 1);
    if ((s16)work->field_89A == 0) {
        tickWork  = arg0->field_1C;
        tickIndex = 1;
        tickSlot  = ((s8*)tickWork + 0x28);
        do {
            tickSlotIndex  = tickIndex;
            tickSlot[0x39] = (u8)tickWork->field_8A2;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickSlot  += 0x28;
            tickIndex += 1;
        } while (tickIndex < 0x13);
    } else {
        ActorsShared80133eb8_BlendTick(arg0);
        if (work->field_4A4 & 1) {
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
        ActorsShared80132808(&arg0->field_2C->coords[5], signedTurn);
        ActorsShared80132808(&arg0->field_2C->coords[2], (s16)clampedAngle / 2);
        arg0->field_2C->coords[5].flg = 0;
        arg0->field_2C->coords[4].flg = 0;
        arg0->field_2C->coords[3].flg = 0;
        arg0->field_2C->coords[2].flg = 0;
    }
    sound = ActorsShared80133eb8_Sound(work);
    if (sound != 0) {
        soundId = sound | (((u16)enemy->placeKey >> 0xC) << 8);
        pan     = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(soundId, (s32)pan,
                            (s32)(s8)gpGetObjDepth(arg0->field_2C->coords));
    }
}
