#include "common.h"

#include "actors/actor_323000.h"
#include "actors/actors_shared_801366fc.h"
#include "actors/actors_shared_80132808.h"
#include "actors/actors_shared_8016331c.h"
#include "gameplay/3CD8.h"
#include "psyq/inline_c.h"

s32 func_actor_323000_80163448(Task* task, Actor323000Work* work)
{
    SVECTOR vec;
    s32     reset;

    reset = 1;
    switch (work->field_82E) {
        case 0: {
            s32 clip = work->slots[9].field_2 & 0x3FF;
            s32 old;

            if (clip == 0x58) {
                old = work->field_848[9];
                if (old != clip) {
                    work->field_848[9] = clip;
                    vec.vx             = -500;
                    vec.vz             = 200;
                    vec.vy             = 650;
                    Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[9], 0x80002220, &vec);
                    return 0x40010002;
                }
                work->field_848[9] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[7].field_2 & 0x3FF;
                s32 old;

                if (clip == 0x3E) {
                    old = work->field_848[7];
                    if (old != clip) {
                        work->field_848[7] = clip;
                        vec.vx             = -1000;
                        vec.vz             = 200;
                        vec.vy             = 650;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80002220, &vec);
                        return 0x40010001;
                    }
                    work->field_848[7] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[14].field_2 & 0x3FF;
                s32 old;

                if (clip == 0x84) {
                    old = work->field_848[14];
                    if (old != clip) {
                        work->field_848[14] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[14], 0x80002220, &vec);
                        return 0x40010002;
                    }
                    work->field_848[14] = old;
                    reset               = 0;
                }
            }
            {
                s32 clip = work->slots[17].field_2 & 0x3FF;
                s32 old;

                if (clip == 0xA9) {
                    old = work->field_848[17];
                    if (old != clip) {
                        work->field_848[17] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[17], 0x80002220, &vec);
                        return 0x40010001;
                    }
                    work->field_848[17] = old;
                    reset               = 0;
                }
            }
            break;
        case 10: {
            s32 clip = work->slots[1].field_2 & 0x3FF;
            s32 old;

            if (clip == 0x9) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0;
                    Gp_SpawnEff(0x60054, ((TmdObject*)task->extra)->field_8, 0x80004A00, &vec);
                    return 0x40010005;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        } break;
        case 3: {
            s32 clip = work->slots[1].field_2 & 0x3FF;
            s32 old;

            if (clip == 0x4) {
                reset = 0;
                old   = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    return 0x40010004;
                }
                work->field_848[1] = old;
            }
        }
            {
                s32 clip = work->slots[1].field_2 & 0x3FF;
                s32 old;

                if (clip == 0x8) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        return 0x40010003;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;
        case 6: {
            s32 clip = work->slots[1].field_2 & 0x3FF;
            s32 old;

            if (clip == 0x6) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vx             = -500;
                    vec.vz             = 200;
                    vec.vy             = 650;
                    Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[9], 0x80003200, &vec);
                    vec.vx = -1000;
                    vec.vz = 200;
                    vec.vy = 650;
                    Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80003200, &vec);
                    return 0;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[1].field_2 & 0x3FF;
                s32 old;

                if (clip == 0xB) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[14], 0x80004480, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[17], 0x80004480, &vec);
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].field_2 & 0x3FF;
                s32 old;

                if (clip == 0xC) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vx             = -500;
                        vec.vz             = 200;
                        vec.vy             = 650;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[9], 0x80002200, &vec);
                        vec.vx = -1000;
                        vec.vz = 200;
                        vec.vy = 650;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80002240, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[14], 0x80003300, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[17], 0x80003340, &vec);
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].field_2 & 0x3FF;
                s32 old;

                if (clip == 0xD) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[14], 0x80002200, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[17], 0x80002300, &vec);
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;
    }
    if (reset == 1) {
        Mem_Set(work->field_848, 0, 0x48);
    }
    return 0;
}

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

void func_actor_323000_80163A30(Task* task)
{
    Actor323000Work* work;
    Actor323000Work* seekWork;
    Actor323000Work* resetWork;
    Actor323000Work* secondaryWork;
    Actor323000Work* tickWork;
    Actor323000Work* turnWork;
    u32              table;
    s16              state;
    s32              seekIndex;
    s32              seekSlotIndex;
    s32              animation;
    s32              index;
    s32              resetIndex;
    s32              resetSlotIndex;
    s32              secondaryIndex;
    s32              secondarySlotIndex;
    s32              tickIndex;
    s32              tickSlotIndex;
    s32              targetAngle;
    s32              currentAngle;
    s32              targetAngleBits;
    s32              currentAngleBits;
    s16              angle;
    s32              clampedAngle;
    s16              thirdAngle;
    s32              targetTurn;
    u16              originalTurn;
    s32              signedTurn;
    s16              currentTurn;
    s32              updatedTurn;
    u16              updatedTurnBits;
    s32              delta;
    s32              sound;
    s32              pan;

    work  = (Actor323000Work*)task->work;
    state = work->field_828;
    if (state == 1) {
        if (work->field_82C != work->field_82E) {
            seekWork = work;
            TOUCH_REG(seekWork);
            seekIndex = 1;
            table     = (u32)D_actor_323000_80173090;
            do {
                seekSlotIndex                  = seekIndex;
                work->slots[seekIndex].field_9 = (u8)seekWork->field_832;
                animation                      = seekWork->field_82E;
                index                          = seekWork->field_82C * 0x2D;
                func_800B4114(&seekWork->anim, seekSlotIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->field_82C = seekWork->field_82E;
        }
        work->field_828 = 3;
        work->field_830 = 0;
        Mem_Set(work->field_848, 0U, 0x48U);
    } else if (state == 2) {
        resetWork = work;
        TOUCH_REG(resetWork);
        resetIndex = 1;
        do {
            resetSlotIndex                  = resetIndex;
            work->slots[resetIndex].field_9 = (u8)resetWork->field_832;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex, (s32)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(work->field_848, 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork            = (Actor323000Work*)task->work;
        secondaryIndex           = 1;
        secondaryWork->field_83A = 0x20;
        secondaryWork->field_83C = 0x800;
        do {
            secondarySlotIndex                           = secondaryIndex;
            secondaryWork->slots[secondaryIndex].field_9 = (u8)secondaryWork->field_83A;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if (work->field_82A == 0) {
        tickWork  = (Actor323000Work*)task->work;
        tickIndex = 1;
        do {
            tickSlotIndex                      = tickIndex;
            tickWork->slots[tickIndex].field_9 = (u8)tickWork->field_832;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        ActorsShared8016331c(task);
        if (work->blendSlots[1].field_10 & 1) {
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
        ActorsShared80132808(&((TmdObject*)task->extra)->field_8[2], thirdAngle);
        ((TmdObject*)task->extra)->field_8[2].flg = 0;
        ActorsShared80132808(&((TmdObject*)task->extra)->field_8[3], thirdAngle);
        ((TmdObject*)task->extra)->field_8[3].flg = 0;
        ActorsShared80132808(&((TmdObject*)task->extra)->field_8[4], (s16)clampedAngle / 2);
        ((TmdObject*)task->extra)->field_8[4].flg = 0;
    }
    turnWork     = (Actor323000Work*)task->work;
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
    ActorsShared80132808(&((TmdObject*)task->extra)->field_8[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    ((TmdObject*)task->extra)->field_8[10].flg = 0;
    sound                                      = func_actor_323000_80163448(task, work);
    if (sound != 0) {
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
    }
}

void func_actor_323000_80163EA0(GpEnemy* enemy, Task* task)
{
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has
    VECTOR           pos;
    TmdObject*       obj;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    Actor323000Work* work;
    Actor323000Work* work2;
    Actor323000Work* mem;

    obj        = (TmdObject*)task->extra;
    coord      = obj->field_8;
    mem        = (Actor323000Work*)Mem_Calloc(0x934, 0);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback = ActorsShared801366fc;
    work2              = (Actor323000Work*)task->work;
    tmd                = (TmdObject*)task->extra;
    tmd->field_1C      = &work2->light;
    tmd->field_20      = &work2->color;
    enemy->field_4     = &((TmdObject*)task->extra)->field_8->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)task->extra)->field_8[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_50     = &D_actor_323000_80164D54;
    enemy->field_4C     = 0;
    enemy->field_40     = 0;
    enemy->field_54     = 0;
    func_800B3F84(&work->anim, D_actor_323000_8017387C, (GpAnimObj*)obj, work->poses, work->slots);
    func_800B3F84(&work->blendAnim, D_actor_323000_8017387C, (GpAnimObj*)obj, work->blendPoses, work->blendSlots);
    work->field_828 = 2;
    work->field_82E = 1;
    work->field_82A = 0;
    work->field_844 = 0;
    work->field_840 = 0;
    work->field_834 = 0x10;
    work->field_832 = 0x10;
    func_actor_323000_80163A30(task);
    task->field_24 = &D_actor_323000_801739D0;
    coord->sub     = &Gfx_ViewCoord;
    coord->flg     = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_323000_80173A24.field_0 = ((TmdObject*)task->extra)->field_8;
    D_actor_323000_80173A24.field_4 = 0x100;
    D_actor_323000_80173A24.field_6 = 2;
    work->field_0                   = 0;
    task->state++;
}

/// State handler: when the live-actor flag is set it clears the enemy's link
/// flag (the reverse of the re-init handler below), drops the model root's
/// `field_C` and rebuilds its buffers, then seeds the animation-state slots
/// before the tick. Otherwise it ticks, and the state the step counter names
/// every `field_68` bit 0 frame spawns the actor's effect 0x60054 at the model
/// root's eighth coordinate with the jump vector (-0x3E8, 0x28A, 0xC8).
void func_actor_323000_8016409C(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    SVECTOR          sp10;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj                 = (TmdObject*)task->extra;
        enemy->node.field_4 = 0;
        obj->field_C        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        func_actor_323000_80163A30(task);
        return;
    }
    func_actor_323000_80163A30(task);
    if (work->slots[1].field_10 & 1) {
        if (work->field_82E == 0xF) {
            work->field_828 = 2;
            work->field_82E = 0x10;
        }
        func_actor_323000_80163A30(task);
    }
    if (work->field_82E == 0xE) {
        if ((work->slots[1].field_2 & 0x3FF) == 7 || (work->slots[1].field_2 & 0x3FF) == 9) {
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80002300, &sp10);
        }
        if ((work->slots[1].field_2 & 0x3FF) == 8) {
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80003400, &sp10);
        }
    }
}

void func_actor_323000_8016420C(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    s32              id;
    s32              pan;
    SVECTOR          ofs2;
    SVECTOR          ofs;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj                 = (TmdObject*)task->extra;
        enemy->node.field_4 = 1;
        obj->field_C        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xE;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        func_actor_323000_80163A30(task);
        return;
    }
    func_actor_323000_80163A30(task);
    switch (++work->field_6) {
        case 29: {
            SVECTOR* p = &ofs;
            p->vx      = -0x1F4;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[9], 0x80005600, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80005A00, p);
        } break;
        case 32: {
            SVECTOR* p = &ofs;
            p->vx      = -0x3E8;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80005A80, p);
            p->vx = -0x1F4;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[9], 0x80006800, p);
            id  = ((enemy->field_8 >> 12) << 8) | 0x4001000D;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
            ofs2.vy = -0x258;
            ofs2.vx = 0;
            ofs2.vz = -0x384;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[1], 0x80005A00, &ofs2);
        } break;
        case 33: {
            SVECTOR* p = &ofs;
            p->vx      = -0x1F4;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[9], 0x80006800, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80006B00, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[7], 0x80004400, p);
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x258;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[14], 0x80003800, p);
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x258;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[17], 0x80004900, p);
            ofs2.vy = -0x2BC;
            ofs2.vx = 0;
            ofs2.vz = -0x258;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->field_8[1], 0x80005A00, &ofs2);
        } break;
    }
    ((TmdObject*)task->extra)->field_8->flg = 0;
}

/// Per-frame driver of the live actor: brings the model root's coordinate up
/// to date, takes its world position as the actor colour, flags a state change
/// in `field_4`, and runs the state handler `field_0` selects from a stack copy
/// of `D_actor_323000_80161E24`. Afterwards it walks the origin of the model's
/// third part coordinate up to `Gfx_ViewCoord` and stores it as the enemy's
/// local position, parented to the view.
void func_actor_323000_801645A4(GpEnemy* enemy, Task* task)
{
    Actor323000Work*        work;
    GpEnemyTaskFuncTable4   sp;
    Actor323000TickScratch* scratch;
    u8*                     head;
    GsCOORDINATE2*          walker;
    SVECTOR*                pos;

    work = (Actor323000Work*)task->work;
    Game_GetPtrSlot(3);
    sp                                      = D_actor_323000_80161E24;
    ((TmdObject*)task->extra)->field_8->flg = 0;
    head                                    = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD                   = head - 0x1C;
    scratch                                 = (Actor323000TickScratch*)(head - 0x1C);
    Gp_UpdateCoord(((TmdObject*)task->extra)->field_8);
    scratch->pos.vx = ((TmdObject*)task->extra)->field_8->workm.t[0];
    scratch->pos.vy = ((TmdObject*)task->extra)->field_8->workm.t[1];
    scratch->pos.vz = ((TmdObject*)task->extra)->field_8->workm.t[2];
    Gp_UpdateActorColor(enemy, &scratch->pos, 0, 0);
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    sp.funcs[work->field_0](enemy, task);
    scratch->local.vx = 0;
    scratch->local.vy = 0;
    scratch->local.vz = 0;
    {
        SVECTOR  local;
        VECTOR   result;
        s32      flag;
        SVECTOR* localp = &local;

        walker   = &((TmdObject*)task->extra)->field_8[2];
        pos      = &scratch->local;
        local.vx = scratch->local.vx;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->sub == NULL)
                break;
            if (walker != &Gfx_ViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(localp);
                __asm__ volatile("nop; nop; .word 0x4A480012");
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->sub;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    enemy->field_1C.vx     = scratch->local.vx;
    enemy->field_1C.vy     = scratch->local.vy;
    enemy->field_1C.vz     = scratch->local.vz;
    enemy->field_18        = &Gfx_ViewCoord;
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

void func_actor_323000_8016483C(void)
{
}
