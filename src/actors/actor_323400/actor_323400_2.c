#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_323400.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"

/// 0x1C-byte block `func_actor_323400_801644C4` pushes on `G_SCRATCH_HEAD`:
/// the model root's world position for `Gp_UpdateActorColor`, and the local
/// point walked up the coordinate chain into view space.
typedef struct Actor323400TickScratch {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR local;
    /* 0x18 */ s32     pad_18;
} Actor323400TickScratch;
STATIC_ASSERT_SIZEOF(Actor323400TickScratch, 0x1C);

/// Per-state animation table `func_actor_323400_80163B58` reads when it
/// re-seeds the slots: 0x2D bytes per `field_82C`, indexed by `field_82E`.
extern s8 D_actor_323400_80170894[];

/// Animation source `func_800B3F84` is handed for both of the work block's
/// contexts.
extern u8 D_actor_323400_80171080[];

/// Message table published as `Task::msgTable` by the spawn handler.
extern void* D_actor_323400_801711D4;

/// Effect record the spawn handler fills: the model root's coordinate and
/// the two spawn arguments 0x100 and 2.
extern GpEffArg D_actor_323400_80171228;

/// Enemy pair source `GpEnemy::param` is pointed at by the spawn handler.
extern GpPairSrcE D_actor_323400_80164D5C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Per-frame effect dispatch keyed on `field_82E` and the record each animation
/// slot has reached. A recognised record is handled once: `field_848` remembers,
/// per slot, the record last handled, and meeting it again only clears `reset`.
/// A handled record spawns its effects while the room effect mode is 2 and
/// returns a request word; otherwise the result is 0, after wiping `field_848`
/// when no case claimed a record.
///
/// `steer` is a matching carrier (see `CSE_STEER`); it has no effect.
s32 func_actor_323400_80163448(Task* task, Actor323400Work* work)
{
    SVECTOR vec;
    s32     reset;
    s32     steer;
    reset = 1;
    switch (work->field_82E) {
        case 0: {
            s32 clip = work->slots[9].curRec & 0x3FF;
            s32 old;
            if (clip == 0x58) {
                old = work->field_848[9];
                if (old != clip) {
                    work->field_848[9] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002220, &vec);
                    }
                    return 0x40010002;
                }
                work->field_848[9] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[7].curRec & 0x3FF;
                s32 old;
                if (clip == 0x3E) {
                    old = work->field_848[7];
                    if (old != clip) {
                        work->field_848[7] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[7] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[14].curRec & 0x3FF;
                s32 old;
                if (clip == 0x84) {
                    old = work->field_848[14];
                    if (old != clip) {
                        work->field_848[14] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80002220, &vec);
                        }
                        return 0x40010002;
                    }
                    work->field_848[14] = old;
                    reset               = 0;
                }
            }
            {
                s32 clip = work->slots[17].curRec & 0x3FF;
                s32 old;
                if (clip == 0xA9) {
                    old = work->field_848[17];
                    if (old != clip) {
                        work->field_848[17] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[17] = old;
                    reset               = 0;
                }
            }
            break;

        case 10: {
            s32 clip = work->slots[1].curRec & 0x3FF;
            s32 old;
            if (clip == 0x9) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, ((TmdObject*)task->extra)->coords, 0x80004A00, &vec);
                    }
                    return 0x40010005;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        } break;

        case 3: {
            s32 clip = work->slots[1].curRec & 0x3FF;
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
                s32 clip = work->slots[1].curRec & 0x3FF;
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
            s32 clip = work->slots[1].curRec & 0x3FF;
            s32 old;
            if (clip == 0x6) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80003200, &vec);
                    }
                    vec.vz = 0;
                    vec.vx = 0;
                    vec.vy = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80003200, &vec);
                    }
                    return 0;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xB) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80004480, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80004480, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xC) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002200, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x2BC;
                        if (steer == 0 && Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002240, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (steer == 0 && Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80003300, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80003340, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xD) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80002200, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80002300, &vec);
                        }
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

/// Per-frame animation tick. Seeds the slots when `field_828` asks for it
/// (1 from the per-state table `D_actor_323400_80170894`, 2 by resetting to
/// `field_82E`), seeds the blend context when `field_836` is 2, then ticks
/// the slots - blended through `func_actor_323400_8016331C` while `field_82A`
/// is set. It eases `field_844` toward `field_840` and spreads it over the
/// body joints 2-4, eases `field_842` toward `field_83E` for joint 10, and
/// plays the sound `func_actor_323400_80163448` returns, panned at the root.
void func_actor_323400_80163B58(Task* task)
{
    Actor323400Work* work;
    Actor323400Work* seekWork;
    Actor323400Work* resetWork;
    Actor323400Work* secondaryWork;
    Actor323400Work* tickWork;
    Actor323400Work* turnWork;
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

    work  = (Actor323400Work*)task->work;
    state = work->field_828;
    if (state == 1) {
        if (work->field_82C != work->field_82E) {
            seekWork = work;
            TOUCH_REG(seekWork);
            seekIndex = 1;
            table     = (u32)D_actor_323400_80170894;
            do {
                seekSlotIndex               = seekIndex;
                work->slots[seekIndex].rate = (u8)seekWork->field_832;
                animation                   = seekWork->field_82E;
                index                       = seekWork->field_82C * 0x2D;
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
            resetSlotIndex               = resetIndex;
            work->slots[resetIndex].rate = (u8)resetWork->field_832;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex, (s32)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(work->field_848, 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork            = (Actor323400Work*)task->work;
        secondaryIndex           = 1;
        secondaryWork->field_83A = 0x20;
        secondaryWork->field_83C = 0x800;
        do {
            secondarySlotIndex                        = secondaryIndex;
            secondaryWork->slots[secondaryIndex].rate = (u8)secondaryWork->field_83A;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if (work->field_82A == 0) {
        tickWork  = (Actor323400Work*)task->work;
        tickIndex = 1;
        do {
            tickSlotIndex                   = tickIndex;
            tickWork->slots[tickIndex].rate = (u8)tickWork->field_832;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        func_actor_323400_8016331C(task);
        if (work->blendSlots[1].flags & 1) {
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
        func_actor_323400_80161E8C(&((TmdObject*)task->extra)->coords[2], thirdAngle);
        ((TmdObject*)task->extra)->coords[2].flg = 0;
        func_actor_323400_80161E8C(&((TmdObject*)task->extra)->coords[3], thirdAngle);
        ((TmdObject*)task->extra)->coords[3].flg = 0;
        func_actor_323400_80161E8C(&((TmdObject*)task->extra)->coords[4], (s16)clampedAngle / 2);
        ((TmdObject*)task->extra)->coords[4].flg = 0;
    }
    turnWork     = (Actor323400Work*)task->work;
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
    func_actor_323400_80161E8C(&((TmdObject*)task->extra)->coords[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    ((TmdObject*)task->extra)->coords[10].flg = 0;
    sound                                     = func_actor_323400_80163448(task, work);
    if (sound != 0) {
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
    }
}

/// Spawn state: allocates the work block (destroying the enemy if that
/// fails), installs the exit callback, binds the model's light and colour
/// matrices to the block, sets up the enemy record and links its node,
/// initialises both animation contexts, seeds clip 1 and ticks once. It then
/// publishes the message table, parents the root to the view, takes its world
/// position as the actor colour, fills the effect record and advances the
/// task to the per-frame driver.
void func_actor_323400_80163FC8(GpEnemy* enemy, Task* task)
{
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has
    VECTOR           pos;
    TmdObject*       obj;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    Actor323400Work* work;
    Actor323400Work* work2;
    Actor323400Work* mem;

    obj        = (TmdObject*)task->extra;
    coord      = obj->coords;
    mem        = (Actor323400Work*)memCalloc(0x934, 0);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback = func_actor_323400_80164A78;
    work2              = (Actor323400Work*)task->work;
    tmd                = (TmdObject*)task->extra;
    tmd->lightMtx      = &work2->light;
    tmd->colorMtx      = &work2->color;
    enemy->field_4     = &((TmdObject*)task->extra)->coords->coord;
    enemy->field_48    = 0;
    enemy->bodyPos.vx  = 0;
    enemy->bodyPos.vy  = 0;
    enemy->bodyPos.vz  = 0;
    enemy->coord       = &((TmdObject*)task->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->param         = &D_actor_323400_80164D5C;
    enemy->reactionFlags = 0;
    enemy->hp            = 0;
    enemy->recs          = 0;
    func_800B3F84(&work->anim, D_actor_323400_80171080, obj, work->poses, work->slots);
    func_800B3F84(&work->blendAnim, D_actor_323400_80171080, obj, work->blendPoses, work->blendSlots);
    work->field_828 = 2;
    work->field_82E = 1;
    work->field_82A = 0;
    work->field_844 = 0;
    work->field_840 = 0;
    work->field_834 = 0x10;
    work->field_832 = 0x10;
    func_actor_323400_80163B58(task);
    task->msgTable = &D_actor_323400_801711D4;
    coord->sub     = &gGfxViewCoord;
    coord->flg     = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_323400_80171228.coord      = ((TmdObject*)task->extra)->coords;
    D_actor_323400_80171228.spawnArgLo = 0x100;
    D_actor_323400_80171228.spawnArgHi = 2;
    work->field_0                      = 0;
    task->state++;
}

void func_actor_323400_801641C4(GpEnemy* enemy, Task* task)
{
    Actor323400Work* work;
    TmdObject*       obj;
    s32              id;
    s32              pan;
    SVECTOR          ofs2;
    SVECTOR          ofs;

    work = (Actor323400Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xD;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        func_actor_323400_80163B58(task);
        return;
    }
    switch (++work->field_6) {
        case 9: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002400, p);
            }
            break;
        }
        case 10: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002400, p);
            }
            id  = ((enemy->placeKey >> 12) << 8) | 0x4001000E;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
            break;
        }
        case 12: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80003600, p);
            }
            break;
        }
        case 13: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80004500, p);
            }
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002480, p);
            }
            ofs2.vy = 0x3E8;
            ofs2.vx = 0;
            ofs2.vz = -0x12C;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[1], 0x80005900, &ofs2);
            break;
        }
    }
    func_actor_323400_80163B58(task);
    ((TmdObject*)task->extra)->coords->flg = 0;
}

/// Per-frame driver of the live actor: brings the model root's coordinate up
/// to date, takes its world position as the actor colour, flags a state change
/// in `field_4`, and runs the state handler `field_0` selects from a stack copy
/// of `D_actor_323400_80161E24`. Afterwards it walks the origin of the model's
/// third part coordinate up to `gGfxViewCoord` and stores it as the enemy's
/// local position, parented to the view.
void func_actor_323400_801644C4(GpEnemy* enemy, Task* task)
{
    Actor323400Work*        work;
    GpEnemyTaskFuncTable4   sp;
    Actor323400TickScratch* scratch;
    u8*                     head;
    GsCOORDINATE2*          walker;
    SVECTOR*                pos;

    work = (Actor323400Work*)task->work;
    gameGetPtrSlot(3);
    sp                                     = D_actor_323400_80161E24;
    ((TmdObject*)task->extra)->coords->flg = 0;
    head                                   = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD                  = head - 0x1C;
    scratch                                = (Actor323400TickScratch*)(head - 0x1C);
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
    scratch->pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    scratch->pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    scratch->pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
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

        walker   = &((TmdObject*)task->extra)->coords[2];
        pos      = &scratch->local;
        local.vx = scratch->local.vx;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->sub == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(localp);
                gte_rtv0tr();
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
    enemy->bodyPos.vx      = scratch->local.vx;
    enemy->bodyPos.vy      = scratch->local.vy;
    enemy->bodyPos.vz      = scratch->local.vz;
    enemy->coord           = &gGfxViewCoord;
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

void func_actor_323400_8016475C(void)
{
}
