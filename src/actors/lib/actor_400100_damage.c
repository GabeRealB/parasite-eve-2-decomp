#include "common.h"

#include "actors/actor_400100.h"
#include "actors/actor_400100_damage.h"
#include "actors/actor_400100_motion.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"
#include "main/fs.h"
#include <psyq/inline_c.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

s32 Actor00100_Fn01EEC(Actor00100* arg0, Actor00100Work* arg1)
{
    Actor00100Work* work;
    u32             prev;
    s32             var_a0 = 1;

    switch ((s16)arg1->field_82E) {
        case 0:
            if ((arg1->field_5A & 0x3FF) == 9) {
                prev = arg1->field_84C;
                if (prev != 9) {
                    arg1->field_84C    = 9;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[17], 0x80002280, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[9], 0x80002120, &work->field_898);
                    }
                    return 0x40010002;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            if ((arg1->field_5A & 0x3FF) == 6) {
                prev = arg1->field_84C;
                if (prev != 6) {
                    arg1->field_84C    = 6;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[14], 0x80002220, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[7], 0x80002120, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            break;
        case 0xA:
            if ((arg1->field_5A & 0x3FF) == 0xA) {
                prev = arg1->field_84C;
                if (prev != 0xA) {
                    arg1->field_84C    = 0xA;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[0], 0x80004A00, &work->field_898);
                    }
                    return 0x40010005;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            break;
        case 3:
            if ((arg1->field_5A & 0x3FF) == 0xC) {
                prev = arg1->field_84C;
                if (prev != 0xC) {
                    arg1->field_84C = 0xC;
                    return 0x40010004;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            if ((arg1->field_5A & 0x3FF) == 8) {
                prev = arg1->field_84C;
                if (prev != 8) {
                    arg1->field_84C = 8;
                    return 0x40010003;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            break;
        case 6:
            if ((arg1->field_5A & 0x3FF) == 6) {
                prev = arg1->field_84C;
                if (prev != 6) {
                    arg1->field_84C    = 6;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[7], 0x80003200, &work->field_898);
                    }
                    return 0x40010004;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            if ((arg1->field_5A & 0x3FF) == 0xC) {
                prev = arg1->field_84C;
                if (prev != 0xC) {
                    arg1->field_84C    = 0xC;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[17], 0x80004480, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[14], 0x80004480, &work->field_898);
                    }
                    return 0x40010011;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            break;
        case 0x12:
            if ((arg1->field_5A & 0x3FF) == 6) {
                prev = arg1->field_84C;
                if (prev != 6) {
                    arg1->field_84C    = 6;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[7], 0x80003200, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            if ((arg1->field_5A & 0x3FF) == 9) {
                prev = arg1->field_84C;
                if (prev != 9) {
                    arg1->field_84C    = 9;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[17], 0x80003200, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            if ((arg1->field_5A & 0x3FF) == 0xE) {
                prev = arg1->field_84C;
                if (prev != 0xE) {
                    arg1->field_84C    = 0xE;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[14], 0x80003200, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[17], 0x80003200, &work->field_898);
                    }
                    return 0x40010002;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            break;
        case 0x11:
            if ((arg1->field_5A & 0x3FF) == 6) {
                prev = arg1->field_84C;
                if (prev != 6) {
                    arg1->field_84C    = 6;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[7], 0x80003200, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            if ((arg1->field_5A & 0x3FF) == 0xA) {
                prev = arg1->field_84C;
                if (prev != 0xA) {
                    arg1->field_84C    = 0xA;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[7], 0x80003200, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[14], 0x80003200, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            if ((arg1->field_5A & 0x3FF) == 0xE) {
                prev = arg1->field_84C;
                if (prev != 0xE) {
                    arg1->field_84C    = 0xE;
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[14], 0x80003200, &work->field_898);
                    }
                    work               = arg0->field_1C;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[17], 0x80003200, &work->field_898);
                    }
                    return 0x40010002;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            break;
        case 0xD:
            if ((arg1->field_5A & 0x3FF) == 0x18) {
                prev = arg1->field_84C;
                if (prev != 0x18) {
                    arg1->field_84C = 0x18;
                    return 0x4001000F;
                }
                arg1->field_84C = prev;
                var_a0          = 0;
            }
            break;
    }
    if (var_a0 == 1) {
        Mem_Set(&arg1->pad_846[2], 0U, 0x48U);
    }
    return 0;
}

void                         Actor00100_Fn001FC(GsCOORDINATE2*, s16);
void                         Actor00100_Fn01D74(Actor00100*);
void                         Gp_UpdateCoord(GsCOORDINATE2*);
void                         func_800B4114(s8*, s32, s16, s32, s32);
extern s8                    Actor00100_D1B6D0;
extern GpEnemyTaskFuncTable4 Actor00100_D001A0;

void Actor00100_Fn02788(Actor00100* arg0)
{
    s32             index;
    u32             table;
    Actor00100Work* seekWork;
    Actor00100Work* resetWork;
    Actor00100Work* turnWork;
    Actor00100Work* secondaryWork;
    Actor00100Work* tickWork;
    Actor00100Work* work;
    s32             targetAngle;
    s32             animation;
    s32             updatedTurn;
    s16             currentTurn;
    s16             thirdAngle;
    s16             state;
    s32             currentAngle;
    s16             angle;
    s32             seekSlotIndex;
    s32             resetSlotIndex;
    s32             secondarySlotIndex;
    s32             tickSlotIndex;
    s32             signedTurn;
    s32             soundId;
    s32             sound;
    s32             resetIndex;
    s32             secondaryIndex;
    s32             tickIndex;
    s32             seekIndex;
    s32             delta;
    s8*             tickSlot;
    s8*             seekSlot;
    s8*             resetSlot;
    s8*             secondarySlot;
    s32             pan;
    s32             currentAngleBits;
    u16             originalTurn;
    s32             targetAngleBits;
    u16             updatedTurnBits;
    s32             clampedAngle;
    s32             targetTurn;

    work  = arg0->field_1C;
    state = (s16)work->field_828;
    if (state == 1) {
        if (work->field_82C != (s16)work->field_82E) {
            seekWork = work;
            TOUCH_REG(seekWork);
            seekIndex = 1;
            table     = (u32)&Actor00100_D1B6D0;
            seekSlot  = (s8*)&work->anim0.field_C;
            do {
                seekSlotIndex  = seekIndex;
                seekSlot[0x39] = (u8)seekWork->field_832;
                animation      = (s16)seekWork->field_82E;
                seekSlot      += 0x28;
                index          = seekWork->field_82C * 0x19;
                func_800B4114(&seekWork->anim0, seekSlotIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->field_82C = (s16)seekWork->field_82E;
        }
        work->field_828 = 3;
        work->field_830 = 0;
        Mem_Set(&work->pad_846[2], 0U, 0x48U);
    } else if (state == 2) {
        resetWork = work;
        TOUCH_REG(resetWork);
        resetIndex = 1;
        resetSlot  = (s8*)&work->anim0.field_C;
        do {
            resetSlotIndex  = resetIndex;
            resetSlot[0x39] = (u8)resetWork->field_832;
            resetSlot      += 0x28;
            Gp_AnimResetSlot(&resetWork->anim0, resetSlotIndex, (s32)(s16)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = (s16)resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(&work->pad_846[2], 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork  = arg0->field_1C;
        secondaryIndex = 1;
        secondarySlot  = (s8*)&secondaryWork->anim0.field_C;
        do {
            secondarySlotIndex  = secondaryIndex;
            secondarySlot[0x39] = (u8)secondaryWork->field_83A;
            secondarySlot      += 0x28;
            Gp_AnimResetSlot(&secondaryWork->anim1, secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if ((s16)work->field_82A == 0) {
        tickWork  = arg0->field_1C;
        tickIndex = 1;
        tickSlot  = (s8*)&tickWork->anim0.field_C;
        do {
            tickSlotIndex  = tickIndex;
            tickSlot[0x39] = (u8)tickWork->field_832;
            Gp_AnimTickIndex(&tickWork->anim0, tickSlotIndex);
            tickSlot  += 0x28;
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        Actor00100_Fn01D74(arg0);
        if (work->field_46C & 0x100) {
            work->field_82A = 0;
        }
    }
    targetAngle      = (s16)work->field_840;
    currentAngle     = (s16)work->field_844;
    targetAngleBits  = work->field_840;
    currentAngleBits = work->field_844;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_844 = currentAngleBits + 0x71;
        } else {
            goto block_26;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_844 = currentAngleBits - 0x71;
    } else {
    block_26:
        work->field_844 = targetAngleBits;
    }
    angle        = (s16)work->field_844;
    clampedAngle = work->field_844;
    if (angle != 0) {
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        Actor00100_Fn001FC(&arg0->field_2C->field_8[2], thirdAngle);
        arg0->field_2C->field_8[2].flg = 0;
        Actor00100_Fn001FC(&arg0->field_2C->field_8[3], thirdAngle);
        arg0->field_2C->field_8[3].flg = 0;
        Actor00100_Fn001FC(&arg0->field_2C->field_8[4], (s16)clampedAngle / 2);
        arg0->field_2C->field_8[4].flg = 0;
    }
    if (((s16)work->field_82E == 0) && (work->field_0 == 0x26)) {
        Gfx_RotMatrixX(&arg0->field_2C->field_8[4].coord, 0x280, 0);
        arg0->field_2C->field_8[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->field_8[4]);
    }
    turnWork     = arg0->field_1C;
    targetTurn   = turnWork->field_83E;
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
    Actor00100_Fn001FC(&arg0->field_2C->field_8[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    arg0->field_2C->field_8[10].flg = 0;
    sound                           = Actor00100_Fn01EEC(arg0, work);
    if (sound != 0) {
        soundId = sound | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
        pan     = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(soundId, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
}

/// Builds the damage state: allocates the 0xC30 work block, wires the two
/// animation contexts and the four collision objects onto the model, latches
/// the spawn position and the one a fixed step ahead of it, then picks the
/// start state and pose row out of the two nibbles of `spawnArg1`.
void Actor00100_Fn02C54(GpEnemy* arg0, Task* arg1)
{
    SVECTOR         vec;
    VECTOR          color;
    u8              cmd30[8];
    u8              cmd38[8];
    Actor00100Work* work;
    TmdObject*      tmd;
    GsCOORDINATE2*  coord;
    Actor00100Work* mapped;
    TmdObject*      model;
    Actor00100Obj*  primary;
    Actor00100Obj*  secondary;
    SVECTOR*        dir;
    s32             kind;
    s32             sessionMode;

    coord       = ((TmdObject*)arg1->extra)->field_8;
    tmd         = arg1->extra;
    work        = Mem_Calloc(0xC30U, false);
    arg1->idMap = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    arg1->exitCallback = Actor00100_Fn0B3B4;
    mapped             = (Actor00100Work*)arg1->idMap;
    model              = (TmdObject*)arg1->extra;
    model->field_1C    = &mapped->field_B80;
    model->field_20    = &mapped->field_BA0;
    arg0->field_4      = &((TmdObject*)arg1->extra)->field_8[0].coord;
    arg0->field_48     = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_18     = &((TmdObject*)arg1->extra)->field_8[2];
    Gp_LinkNode(&arg0->node);
    arg0->node.field_4 = 1;
    arg0->field_4C     = 0;
    arg0->field_40     = Actor00100_D0BDA4.field_4;
    arg0->field_50     = &Actor00100_D0BDA4;
    arg0->field_54     = (s32)&work->objs[0].field_20;
    func_800B3F84(&work->anim0, &Actor00100_D1B944, (GpAnimObj*)tmd, work->data0, &work->slot0);
    func_800B3F84(&work->anim1, &Actor00100_D1B944, (GpAnimObj*)tmd, work->data1, &work->slot1);
    work->field_828 = 2;
    work->field_82A = 0;
    work->field_82E = 0;
    work->field_844 = 0;
    work->field_840 = 0;
    if (((u16)arg0->field_8 >> 0xC) & 1) {
        work->field_834 = 0xF;
        work->field_832 = 0xF;
    } else {
        work->field_834 = 0x11;
        work->field_832 = 0x11;
    }
    work->field_83A = 0x10;
    Actor00100_Fn02788((Actor00100*)arg1);
    work->objs[2].coord    = coord;
    work->objs[2].hits     = &work->objs[2].field_20;
    work->objs[2].field_10 = 0;
    work->objs[2].field_12 = -0x11C;
    work->objs[2].field_14 = 0;
    work->objs[2].field_18 = 0x30001;
    work->objs[2].field_1C = 0x12C;
    work->objs[2].flags    = 1;
    Gp_LinkObj(2, (GpObj*)&work->objs[2]);
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_0  = 0;
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_2  = -0x180;
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_4  = 0;
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_8  = 0;
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_A  = -0x180;
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_C  = 0x2BC;
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_10 = 0x12C;
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_12 = 0x12C;
    ((GpActorD4Rec*)&work->objs[3].field_20)->field_14 = &work->objs[3].field_38;
    work->objs[3].coord                                = coord;
    work->objs[3].hits                                 = &work->objs[3].field_20;
    work->objs[3].field_10                             = 0;
    work->objs[3].field_12                             = 0;
    work->objs[3].field_14                             = 0;
    work->objs[3].field_18                             = 0x30001;
    work->objs[3].field_1C                             = 1;
    work->objs[3].flags                                = 3;
    work->objs[2].flags                               |= 0x4000;
    Gp_LinkObj(2, (GpObj*)&work->objs[3]);
    work->objs[3].flags |= 0x4000;
    Gp_InitRec18Table(&work->objs[3].field_38, 5, 0);
    Gp_InitRec18Table(work->objs[2].hits, 5, 0);
    primary           = &work->objs[0];
    primary->coord    = &((TmdObject*)arg1->extra)->field_8[2];
    primary->hits     = &primary->field_20;
    primary->field_10 = 0;
    primary->field_12 = 0;
    primary->field_14 = 0;
    primary->field_18 = 0x30001;
    primary->field_1C = 0x19C;
    primary->flags    = 1;
    Gp_LinkObj(2, (GpObj*)primary);
    primary->flags |= 0x8000;
    Gp_InitRec18Table(primary->hits, 5, 0);
    secondary           = &work->objs[1];
    secondary->coord    = &((TmdObject*)arg1->extra)->field_8[10];
    secondary->hits     = &secondary->field_20;
    secondary->field_10 = 0;
    secondary->field_12 = 0;
    secondary->field_14 = 0;
    secondary->field_18 = 0x30001;
    secondary->field_1C = 0x100;
    secondary->flags    = 1;
    Gp_LinkObj(2, (GpObj*)secondary);
    secondary->flags |= 0x8000;
    Gp_InitRec18Table(secondary->hits, 5, 0);
    work->objs[1].field_10 = 0;
    work->objs[1].field_12 = 0;
    work->objs[1].field_14 = -0x100;
    work->field_14         = 0;
    work->field_C          = ((TmdObject*)arg1->extra)->field_8[0].coord.t[0];
    work->field_E          = ((TmdObject*)arg1->extra)->field_8[0].coord.t[2];
    Gfx_MatrixCol2(&((TmdObject*)arg1->extra)->field_8[0].coord, &vec);
    vec.vy = 0;
    dir    = &vec;
    VectorNormalSS(dir, dir);
    gte_lddp(5000);
    gte_ldsv(dir);
    gte_gpf12_real();
    gte_stsv(dir);
    work->field_10  = ((TmdObject*)arg1->extra)->field_8[0].coord.t[0] + vec.vx;
    work->field_12  = ((TmdObject*)arg1->extra)->field_8[0].coord.t[2] + vec.vz;
    work->field_BF8 = NULL;
    work->field_BFC = 1;
    work->field_C00 = 0;
    work->field_C04 = 3;
    work->field_C08 = 1;
    arg1->field_24  = &Actor00100_D1BA54;
    coord->sub      = &Gfx_ViewCoord;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    color.vx = coord->workm.t[0];
    color.vy = coord->workm.t[1];
    color.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &color, 0, 0);
    work->field_890.field_0 = &((TmdObject*)arg1->extra)->field_8[1];
    work->field_890.field_4 = 0x100;
    work->field_890.field_6 = 2;
    kind                    = (arg1->spawnArg1 >> 16) & 0xF;
    if (kind == 1) {
        goto state1;
    }
    if (kind < 2) {
        goto stateStill;
    }
    if (kind == 2) {
        goto state2;
    }
    if (kind == 3) {
        goto state3;
    }
    work->field_2 = -1;
    work->field_0 = 0x18;
    Tmd_AllocBuffers(tmd);
    goto stateEnd;
state1:
    work->field_2 = -1;
    work->field_0 = 0;
    goto stateEnd;
state2:
    work->field_2 = -1;
    work->field_0 = 0x21;
    goto stateEnd;
state3:
    work->field_2 = -1;
    work->field_0 = 5;
    goto stateEnd;
stateStill:
    work->field_2 = -1;
    work->field_0 = 0x18;
    Tmd_AllocBuffers(tmd);
stateEnd:
    kind = arg1->spawnArg1 & 0xF;
    if (kind == 1) {
        goto pose2;
    }
    if (kind < 2) {
        goto pose1;
    }
    if (kind != 2) {
        goto pose1;
    }
    work->field_C1E = Actor00100_D0BDB4.rows[0].vy;
    work->field_C20 = Actor00100_D0BDB4.rows[0].vx;
    work->field_C22 = Actor00100_D0BDB4.rows[0].vz;
    work->field_C24 = Actor00100_D0BDB4.rows[0].yaw;
    goto poseEnd;
pose2:
    work->field_C1E = Actor00100_D0BDB4.rows[2].vy;
    work->field_C20 = Actor00100_D0BDB4.rows[2].vx;
    work->field_C22 = Actor00100_D0BDB4.rows[2].vz;
    work->field_C24 = Actor00100_D0BDB4.rows[2].yaw;
    goto poseEnd;
pose1:
    work->field_C1E = Actor00100_D0BDB4.rows[1].vy;
    work->field_C20 = Actor00100_D0BDB4.rows[1].vx;
    work->field_C22 = Actor00100_D0BDB4.rows[1].vz;
    work->field_C24 = Actor00100_D0BDB4.rows[1].yaw;
poseEnd:
    sessionMode = Game_Session->field_7;
    if ((sessionMode - 2) < 2U) {
        if (Game_Session->field_6 == 0x18) {
            cmd38[3] = sessionMode;
            cmd38[2] = Game_Session->field_6;
            cmd38[0] = 0x31;
            cmd30[0] = (u8)Game_Session->field_74;
            cmd30[3] = 0;
            cmd30[2] = 0;
            cmd30[1] = 0;
            CdCmd_Enqueue(0x21, cmd38, cmd30);
        }
    }
    if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x04010000) {
        func_801811C4(0x7D0);
    }
    Gp_ClearRec18Occupied(&work->objs[2].field_20);
    Gp_ClearRec18Occupied(&work->objs[0].field_20);
    Gp_ClearRec18Occupied(&work->objs[1].field_20);
    Gp_ClearRec18Occupied(&work->objs[3].field_38);
    work->field_C2A = 0;
    arg1->state    += 1;
}

/// Picks one of twelve hit positions out of `Actor00100_D1B9F4` by damage
/// magnitude `arg1`, then spawns effect `Gp_GetIdParam1(arg2)` on the model
/// part that entry names.
void Actor00100_Fn03340(Actor00100* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*              sc;
    s32                   mag;
    Actor00100DamageWork* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = (Actor00100DamageWork*)arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = Actor00100_D1B9F4[0];
                break;
            case 1:
                *sc = Actor00100_D1B9F4[1];
                break;
            case 2:
                *sc = Actor00100_D1B9F4[2];
                break;
            case 3:
                *sc = Actor00100_D1B9F4[3];
                break;
            default:
                *sc = Actor00100_D1B9F4[4];
                break;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = Actor00100_D1B9F4[5];
                break;
            case 1:
                *sc = Actor00100_D1B9F4[6];
                break;
            default:
                *sc = Actor00100_D1B9F4[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = Actor00100_D1B9F4[8];
        } else {
            *sc = Actor00100_D1B9F4[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = Actor00100_D1B9F4[10];
        } else {
            *sc = Actor00100_D1B9F4[11];
        }
    }
    work->field_890.field_0 = &arg0->field_2C->field_8[sc->pad];
    work->field_890.field_4 = 0x100;
    work->field_890.field_6 = 2;
    work->field_8A0         = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->field_8[sc->pad], &work->field_8A0, &work->field_890);
    *(u32*)G_SCRATCH_HEAD += 8;
}

void Actor00100_Fn0375C(Actor00100* arg0)
{
    PlayerStatus*            config = &Player_Status;
    Actor00100Ctx*           ctx;
    GsCOORDINATE2*           coord;
    s16                      effect;
    s16                      delta;
    s16                      z;
    s16                      state3;
    s32                      state4;
    s16                      damageState;
    s16                      deathState;
    s16                      hurtState;
    s16                      poisonState;
    s16                      state0;
    s16                      state1;
    s32                      magnitude;
    s16                      nextState;
    s16                      wrapped;
    s32                      yaw;
    s32                      deathSound;
    s32                      hurtSound;
    s32                      hitSound;
    s32                      distance;
    s32                      dx;
    s32                      dy;
    s32                      dz;
    s32                      soundBase;
    s32                      deathPan;
    s32                      hurtPan;
    s32                      hitPan;
    u16                      totalDamage;
    u32                      tickDamage;
    u32                      doubleDamage;
    u32                      kind;
    u8                       sessionMode;
    void*                    hitPos;
    void*                    temp_a2;
    Actor00100DamageWork*    work;
    Actor00100DamageScratch* scratch;
    Actor00100DamageScratch* head;
    void*                    temp_v1_2;
    void*                    temp_v1_3;

    ctx  = arg0->field_20;
    work = (Actor00100DamageWork*)arg0->field_1C;
    if (((s16)ctx->field_40) > 0) {
        head              = *(Actor00100DamageScratch**)G_SCRATCH_HEAD;
        scratch           = (*(Actor00100DamageScratch**)G_SCRATCH_HEAD = (Actor00100DamageScratch*)head - 1);
        scratch->field_20 = Actor00100_FindDamageHit(work->primaryHits, (SVECTOR*)&scratch->field_18);
        if (scratch->field_20 == 0) {
            hitPos            = &scratch->field_18;
            scratch->field_20 = Actor00100_FindDamageHit(work->secondaryHits, (SVECTOR*)hitPos);
        }
        if (scratch->field_20 != 0) {
            work->field_BE4              = 1;
            scratch->field_2E            = -1;
            work->field_BE0              = Gp_GetIdParam2(scratch->field_20);
            arg0->field_2C->field_8->flg = 0;
            Gp_UpdateCoord(arg0->field_2C->field_8);
            scratch->field_10 = (s16)(scratch->field_18 - arg0->field_2C->field_8->workm.t[0]);
            scratch->field_12 = (s16)(scratch->field_1A - arg0->field_2C->field_8->workm.t[1]);
            z                 = scratch->field_1C - arg0->field_2C->field_8->workm.t[2];
            scratch->field_14 = z;
            yaw               = ratan2((s32)scratch->field_10, (s32)z);
            coord             = arg0->field_2C->field_8;
            delta             = yaw - ratan2((s32)-coord->workm.m[2][0], (s32)coord->workm.m[2][2]);
            wrapped           = delta;
            scratch->field_2C = delta;
            if (delta < 0) {
                while (1) {
                    if (wrapped >= -0x800)
                        break;
                    wrapped += 0x1000;
                }
            } else {
                while (1) {
                    if (wrapped <= 0x800)
                        break;
                    wrapped -= 0x1000;
                }
            }
            scratch->field_2C = wrapped;
            kind              = Gp_GetIdParam0(scratch->field_20) & 0xFFFF;
            switch (kind) {
                case 0:
                case 5:
                case 6:
                case 7:
                    state0 = work->field_0;
                    if ((state0 == 0x18) || (state0 == 0x26) || (state0 == 0x20)) {
                        work->field_0 = 0x1C;
                    }
                    if (work->field_0 == 0x21) {
                        work->field_0 = 0x22;
                    }
                    if (work->field_82A == 0) {
                        work->field_BE2 = 0U;
                    }
                    state1 = work->field_0;
                    if (state1 == 4 || state1 == 7 || state1 == 11 || state1 == 17) {
                        work->field_0 = 11;
                        work->field_2 = -1;
                    } else if (state1 != 0x22 && state1 != 0x15 && state1 != 0x14 && state1 != 7 && state1 != 0x24) {
                        work->field_82A = 1;
                        work->field_838 = 15;
                        work->field_836 = 2;
                    }
                    break;
                case 8:
                    sessionMode = gGameSession->at4.loc.stage;
                    if ((sessionMode != 2) && (sessionMode != 5)) {
                        magnitude = scratch->field_2C;
                        if (magnitude < 0) {
                            magnitude = -magnitude;
                        }
                        if (magnitude < 0x501) {
                            Actor00100_SetHitState(work);
                        }
                    }
                    break;
                case 9:
                    Actor00100_SetHitState(work);
                    break;
                case 2:
                    Actor00100_SetHitState(work);
                    Gp_SetObjFlag2((GpObj5D*)ctx, scratch->field_20, 0);
                    break;
                case 3:
                    state3 = work->field_0;
                    if ((state3 == 0x18) || (state3 == 0x26) || (state3 == 0x20)) {
                        work->field_0 = 0x1C;
                    }
                    if (work->field_0 == 0x21) {
                        work->field_0 = 0x22;
                    }
                    Gp_SetObjFlag4((GpObj5C*)ctx, scratch->field_20, 0);
                    break;
                case 1:
                case 4:
                    state4 = work->field_0;
                    if (state4 == 4 || state4 == 7 || state4 == 0x21 || state4 == 0x14 || state4 == 0xB || state4 == 0x11 || (state4 == 0x24 && work->field_6 < 10)) {
                        nextState     = 7;
                        work->field_0 = nextState;
                    } else if (state4 != 0x15 && state4 != 0) {
                        nextState     = 0x14;
                        work->field_0 = nextState;
                    }
                    break;
            }
            dx                = config->coordMtx->t[0] - arg0->field_2C->field_8->coord.t[0];
            scratch->field_0  = dx;
            dy                = config->coordMtx->t[1] - arg0->field_2C->field_8->coord.t[1];
            scratch->field_4  = dy;
            dz                = config->coordMtx->t[2] - arg0->field_2C->field_8->coord.t[2];
            scratch->field_8  = dz;
            distance          = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
            scratch->field_28 = distance;
            scratch->field_24 = Gp_ComputeDamage((u32)scratch->field_20, (u32)distance, 0, 0);
            Actor00100_Fn03340(arg0, scratch->field_2C, scratch->field_20);
            work->field_844 = 0;
            work->field_840 = 0;
            if (Gp_RollEnemyChance((GpEnemy*)ctx, (u32)scratch->field_20, 0) != 0) {
                scratch->field_2E = 0;
                scratch->field_24 = (u32)(scratch->field_24 * 4);
            }
            damageState = work->field_0;
            if (damageState == 4 || (damageState == 0x14 && work->field_6 >= 11) || damageState == 7 || damageState == 0x11 || damageState == 0xB || (damageState == 0x24 && work->field_6 < 10)) {
                doubleDamage      = scratch->field_24 * 2;
                scratch->field_24 = doubleDamage;
                if (doubleDamage != 0) {
                    scratch->field_2E = 3;
                }
            }
            func_800E2C78((GpObj40*)ctx, scratch->field_20, (s32)scratch->field_24, 0);
            effect = scratch->field_2E;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, arg0->field_2C->field_8 + 2, (s32)effect, NULL);
            }
            ctx->field_40 = (s16)((u16)((s16)ctx->field_40) - (u16)scratch->field_24);
            func_800DA6E8(ctx->field_10, (s32)scratch->field_24, 0);
            totalDamage     = work->field_BE2 + (u16)scratch->field_24;
            work->field_BE2 = totalDamage;
            if (((s16)ctx->field_40) <= 0) {
                work->field_904 = 9;
                work->field_905 = 1;
                work->field_906 = 4;
                Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&work->field_904, 0x7DB);
                if ((Gp_GetIdParam0(scratch->field_20) & 0xFFFF) == 4) {
                    work->field_0 = 3;
                } else {
                    deathState = work->field_0;
                    if ((deathState == 0x21) || (deathState == 0x11) || (deathState == 0xB) || (deathState == 7) || (deathState == 4)) {
                        soundBase     = 0x40010008;
                        work->field_0 = 7;
                        work->field_2 = -1;
                        goto playHitSound;
                    }
                    deathSound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x40010008;
                    deathPan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                    SndEvt_EnqueueType6(deathSound, (s32)deathPan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
                    work->field_0 = 0x14;
                }
            } else if ((s16)totalDamage >= 0x47) {
                hurtState = work->field_0;
                if ((hurtState != 4) && (hurtState != 0x14) && (hurtState != 7) && (hurtState != 0xB) && (hurtState != 0x11)) {
                    hurtSound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x40010008;
                    hurtPan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                    SndEvt_EnqueueType6(hurtSound, (s32)hurtPan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
                    work->field_0 = 0x14;
                } else {
                    goto normalHitSound;
                }
            } else {
            normalHitSound:
                soundBase = 0x40010007;
            playHitSound:
                hitSound = (((u16)ctx->field_8 >> 0xC) << 8) | soundBase;
                hitPan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                SndEvt_EnqueueType6(hitSound, (s32)hitPan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
            }
        }
        if (ctx->field_4C & 0xC) {
            scratch->field_24 = Gp_TickObjFlag4((GpObj5C*)ctx);
            if (Gp_ObjFlag4Expired((GpObj5C*)ctx) != 0) {
                ctx->field_4C &= 0xF3;
            }
            ctx->field_40 = (s16)((u16)((s16)ctx->field_40) - (u16)scratch->field_24);
            tickDamage    = scratch->field_24;
            if (tickDamage != 0) {
                func_800DA6E8(ctx->field_10, (s32)tickDamage, 0);
                if (((s16)ctx->field_40) <= 0) {
                    poisonState = work->field_0;
                    if ((poisonState != 4) && (poisonState != 7) && (poisonState != 0xB) && (poisonState != 0x11)) {
                        work->field_0 = 0xA;
                    } else {
                        work->field_0 = 0x15;
                    }
                } else {
                    if (work->field_0 == 0x1C) {
                        work->field_0 = 0x26;
                    }
                    if (work->field_0 != 4 && work->field_0 != 7 && work->field_0 != 11 && work->field_0 != 17) {
                        work->field_82A = 1;
                        work->field_838 = 15;
                        work->field_836 = 2;
                    } else if (work->field_0 != 4) {
                        work->field_0 = 11;
                    } else {
                        work->field_2 = -1;
                    }
                }
            }
        }
        if (((s16)ctx->field_40) <= 0) {
            work->field_C2A = 1;
        }
        *(Actor00100DamageScratch**)G_SCRATCH_HEAD += 1;
    }
}
