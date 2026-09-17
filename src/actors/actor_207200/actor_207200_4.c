#include "common.h"

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "actors/actor_207200.h"
#include "actors/actors_shared_80134700.h"
#include "actors/actors_shared_8013851c.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_207200_80149E30;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern GpU16Pair  D_actor_207200_8014E7CC;
extern GpPairSrcE D_actor_207200_8014E7D4;
extern s32        D_actor_207200_80153ED4;

extern u8  D_801153F2[2];
extern u32 Gp_LcgState;

void func_actor_207200_8014B278(GpEnemy* arg0, Task* arg1)
{
    Actor207200SpawnWork* work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part6;
    GsCOORDINATE2*        part3;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0x4ACU, false);
    part6 = coord + 6;
    part3 = coord + 3;
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap    = (TaskIdMap*)work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_1C  = &work->field_1BC;
    obj->field_20  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18     = coord;
    arg0->node.field_4 = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &D_actor_207200_8014E7D4;
    arg0->field_54     = (s32)work->rec3;
    arg0->field_40     = (u16)D_actor_207200_8014E7D4.field_4;
    work->field_44E    = ((GpCoordPose*)coord)->field_46;
    func_800B3F84((GpAnimCtx*)work, &D_actor_207200_80153ED4, (GpAnimObj*)obj,
                  work->field_12C, (GpAnimSlot*)work->field_14);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);

    work->field_48C = 1;
    work->field_48E = 1;
    work->field_4A4 = 0;
    work->field_4A6 = 0;
    work->field_488 = 0;
    work->field_494 = 0;
    work->field_49E = 0;

    work->obj1.field_8  = coord;
    work->obj1.field_C  = work->rec1;
    work->obj1.field_10 = 0;
    work->obj1.field_12 = 0;
    work->obj1.field_14 = 0;
    work->obj1.field_18 = 0;
    work->obj1.field_1C = 0x7D0;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 1, 0);

    work->obj2.field_12 = -0x12C;
    work->obj2.field_14 = -0xB4;
    work->obj2.field_8  = coord;
    work->obj2.field_C  = work->rec2;
    work->obj2.field_10 = 0;
    work->obj2.field_18 = 0x3002B;
    work->obj2.field_1C = 0x12C;
    work->obj2.flags    = 1;
    work->obj1.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->rec2, 6, 0);

    work->obj3.field_8  = part3;
    work->obj3.field_C  = work->rec3;
    work->obj3.field_10 = 0;
    work->obj3.field_12 = 0;
    work->obj3.field_14 = 0;
    work->obj3.field_18 = 0x3002B;
    work->obj3.field_1C = 0x96;
    work->obj3.flags    = 1;
    work->obj2.flags   |= 0xC200;
    Gp_LinkObj(2, &work->obj3);
    Gp_InitRec18Table(work->rec3, 6, 0);

    work->obj4.field_8  = part3;
    work->obj4.field_C  = work->rec4;
    work->obj4.field_10 = 0;
    work->obj4.field_12 = 0x50;
    work->obj4.field_14 = 0x8C;
    work->obj3.flags   |= 0xC000;
    work->obj4.field_18 = Gp_PackPair(&D_actor_207200_8014E7CC, 0);
    work->obj4.field_1C = 0x12C;
    work->obj4.flags    = 1;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec4, 1, 0);

    work->obj5.field_8  = part6;
    work->obj5.field_C  = work->rec5;
    work->obj5.field_10 = 0xFA;
    work->obj5.field_12 = 0;
    work->obj5.field_14 = 0;
    work->obj4.flags   &= 0x7FFF;
    work->obj5.field_18 = Gp_PackPair(&D_actor_207200_8014E7CC, 1);
    work->obj5.field_1C = 0x12C;
    work->obj5.flags    = 1;
    Gp_LinkObj(3, &work->obj5);
    Gp_InitRec18Table(work->rec5, 1, 0);
    work->obj5.flags &= 0x7FFF;

    work->eff0.field_0 = ((TmdObject*)arg1->extra)->field_8 + 3;
    work->eff0.field_4 = 0x100;
    work->eff0.field_6 = 1;
    work->eff2.field_0 = ((TmdObject*)arg1->extra)->field_8 + 3;
    work->eff2.field_4 = 0x400;
    work->eff2.field_6 = 3;
    work->eff1.field_0 = ((TmdObject*)arg1->extra)->field_8 + 1;
    work->eff1.field_4 = 0x100;
    work->eff1.field_6 = 1;
    work->field_4A8    = 0;
    arg1->exitCallback = (void (*)(Task*))func_actor_207200_8014DB4C;
    arg1->state++;
}

/// Helper-slot state 0 of the enemy: while it is still alive, a hit recorded in
/// the first render node's table (or the global flag `D_801153F2[1]`) arms the
/// death sequence - helper state 1, a random 0..89 delay in `field_4AA` and
/// `Gp_ArmStateF0(1)`. Then runs the idle cycle in `field_48C`: state 1 waits
/// 0x5B frames and rolls a 30% chance of moving to 9, which plays the
/// room-tagged sound on frame 5 and returns to 1 after 0x2D frames.
void func_actor_207200_8014B628(Task* arg0)
{
    Actor207200Work* work;
    GpObj38*         obj;
    s32              id;
    s32              pan;
    u32              rnd;
    u16              hi;

    *(u8**)G_SCRATCH_HEAD -= 8;
    work                   = (Actor207200Work*)arg0->idMap;
    obj                    = (GpObj38*)((TmdObject*)arg0->extra)->field_8;
    if (work->field_4A6 == 0) {
        if (Gp_CountRec18Hi((GpRec18*)work->field_1DC.field_20, 0x10000) != 0) {
            work->field_4A2 = 1;
        }
        if (work->field_4A2 != 0 || D_801153F2[1] != 0) {
            rnd                        = Gp_LcgState * 5 + 0x71357911;
            hi                         = rnd >> 16;
            work->field_49A            = 0;
            work->field_492            = 0;
            work->field_486            = 1;
            Gp_LcgState                = rnd;
            work->field_1DC.obj.flags &= 0x7FFF;
            work->field_4AA            = hi % 90;
            Gp_ArmStateF0(1);
        }
        Gp_ClearRec18Occupied((GpRec18*)work->field_1DC.field_20);
    }
    switch (work->field_48C) {
        case 1:
            work->field_498 = 1;
            work->field_492 = 0;
            if ((s16)work->field_490 >= 0x5B) {
                work->field_490 = 0;
                work->field_48E = 0;
                if (work->field_4A6 == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((u16)((Gp_LcgState >> 16) % 100) < 30) {
                        work->field_48C = 9;
                    }
                }
            }
            break;
        case 9:
            if ((s16)work->field_490 == 5) {
                id  = ((((Actor207200Ctx*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x40480004;
                pan = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(obj));
            }
            if ((s16)work->field_490 >= 0x2D) {
                work->field_48C = 1;
                work->field_490 = 0;
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_4", func_actor_207200_8014B87C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_4", func_actor_207200_8014BEF4);

/// Ticks the shatter timers the enemy runs while it dies. Every time a timer
/// runs out the work is armed with a fresh sound effect - one per stage of the
/// death animation - and the frame it is handed plays.
void func_actor_207200_8014C870(Actor207200* arg0, s32 arg1)
{
    Actor207200Work* work;
    Actor207200Ctx*  ctx;
    GsCOORDINATE2*   coord;
    GpEffArg*        effArg;
    s32              snd;

    work  = arg0->field_1C;
    ctx   = arg0->field_20;
    coord = (*(TmdObject**)&arg0->field_2C)->field_8;

    ctx->field_40 = (s16)((u16)ctx->field_40 - arg1);
    func_800DA6E8(&ctx->node, arg1, 0);
    if ((s16)ctx->field_40 <= 0) {
        if (work->field_4A6 == 0) {
            ctx->field_40 = 1;
            snd           = ((arg0->field_20->field_8 >> 12) << 8) | 0x40480003;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            effArg = &work->field_3F4;
            func_800FDB18(5, (*(TmdObject**)&arg0->field_2C)->field_8 + 3, &D_actor_207200_80153F18, effArg);
            func_800FDB18(5, (*(TmdObject**)&arg0->field_2C)->field_8 + 3, &D_actor_207200_80153F18, effArg);
            work->field_374.obj.flags &= 0x7FFF;
            work->field_3AC.obj.flags &= 0x7FFF;
            Gp_UnlinkObj(&work->field_2C4.obj);
            work->field_4A6 = 1;
            ctx->field_54   = (s32)&work->field_214.field_20[0];
            work->field_488 = 0;
            arg0->field_2A  = 0x14;
        }
    } else {
        if (work->field_48C == 1) {
            snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x40480006;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            work->field_48C = 5;
            work->field_490 = 0;
            work->field_486 = 4;
            work->field_492 = 0;
            work->field_4A2 = 0;
            return;
        }
        snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x40480001;
        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
}

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_207200_8014D7E8(Actor207200* arg0);
void func_actor_207200_8014D97C(Actor207200* arg0, GsCOORDINATE2* arg1);

/// Global mode byte shared by the enemy actors: 1 skips the per-frame update,
/// 2 puts the model in its hidden pose.
extern u8 D_801153F4;

/// `func_actor_207200_8014D65C`'s body, inlined: re-arm the six helper slots
/// when the animation id changed, otherwise advance them by one frame.
static __inline__ void Actor207200_TickAnim(Actor207200* arg0)
{
    Actor207200Work* work;
    s32              i;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_48C != (s16)work->field_48E) {
        work->field_48E = work->field_48C;
        work->field_490 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_48C, 0, 8);
            i++;
        } while (i < 7);
        return;
    }
    TOUCH_REG(i);
    work->field_490 = (u16)(work->field_490 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 7);
}

/// `ActorsShared8013a2c0`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
static __inline__ void Actor207200_UpdateColor(GpEnemy* enemy, Actor207200* actor)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)actor->field_2C)->field_8[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Teardown tick. Mode 2 of `D_801153F4` hides the model, mode 1 does nothing;
/// otherwise the teardown stage in `field_488` advances: 0 releases the actor's
/// state reference, snapshots the model transform and unlinks its node and
/// five display objects; 1 moves on once animation 5 has run 100 frames (or
/// at once for any other animation or once `field_4A8` is set); 2 counts 60
/// frames, spawning an effect on frame 15; 3 destroys the enemy. Every stage
/// but the last then ticks the animation, the attach coordinates and the colour.
void func_actor_207200_8014CA84(GpEnemy* arg0, Actor207200* arg1)
{
    Actor207200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s16              state;

    obj   = (TmdObject*)arg1->field_2C;
    work  = arg1->field_1C;
    coord = obj->field_8;
    switch (D_801153F4) {
        case 1:
            break;
        case 2:
            obj->field_C      |= 0x80;
            arg0->node.field_4 = 1;
            break;
        case 0:
        default:
            state = work->field_488;
            switch (state) {
                case 0:
                    Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x2B);
                    work->field_488 = 1;
                    work->field_48A = 0;
                    work->field_49C = 0x1000;
                    work->field_464 = coord->coord;
                    arg0->field_54  = 0;
                    Gp_UnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->field_1DC.obj);
                    Gp_UnlinkObj(&work->field_214.obj);
                    Gp_UnlinkObj(&work->field_2C4.obj);
                    Gp_UnlinkObj(&work->field_374.obj);
                    Gp_UnlinkObj(&work->field_3AC.obj);
                    break;
                case 1:
                    if (work->field_4A8 == 0) {
                        if (work->field_48C == 5) {
                            if ((s16)work->field_490 >= 100) {
                                work->field_488 = 2;
                            }
                        } else {
                            work->field_488 = 2;
                        }
                    } else {
                        obj->field_C    = 0x80;
                        work->field_488 = 2;
                    }
                    break;
                case 2:
                    work->field_48A++;
                    if ((s16)work->field_48A >= 0x3D) {
                        work->field_488 = 3;
                    }
                    if (work->field_4A8 == 0) {
                        func_actor_207200_8014D7E8(arg1);
                        if ((s16)work->field_48A == 0xA) {
                            obj->field_C = 2;
                        }
                        if ((s16)work->field_48A == 0xF) {
                            Gp_SpawnEff(0x600A5, coord, 2, NULL);
                        }
                    }
                    break;
                case 3:
                    Gp_DestroyEnemy(arg0, (Task*)arg1);
                    return;
            }
            Actor207200_TickAnim(arg1);
            func_actor_207200_8014D97C(arg1, &((TmdObject*)arg1->field_2C)->field_8[2]);
            func_actor_207200_8014D97C(arg1, &((TmdObject*)arg1->field_2C)->field_8[3]);
            ((TmdObject*)arg1->field_2C)->field_8[0].flg = 0;
            ((TmdObject*)arg1->field_2C)->field_8[1].flg = 0;
            Gp_UpdateCoord(&((TmdObject*)arg1->field_2C)->field_8[1]);
            Actor207200_UpdateColor(arg0, arg1);
            break;
    }
}
