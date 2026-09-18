#include "common.h"
#include <psyq/abs.h>

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "actors/actor_207200.h"
#include "actors/actors_shared_80134700.h"
#include "actors/actors_shared_80136614.h"
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
/// `field_492` value for frames 20..39 of helper stage 1, indexed by frame - 20.
extern s16 D_actor_207200_80153F20[];
/// Base damage the shatter hit doubles, before a 0..99 roll is added.
extern u16 D_actor_207200_8014E7D8;
/// Effect offsets `func_800FDB18` is handed for the two hit tables.
extern SVECTOR D_actor_207200_80153F08;
extern SVECTOR D_actor_207200_80153F10;

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
    arg1->work     = (TaskIdMap*)work;
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
    work                   = (Actor207200Work*)arg0->work;
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

/// Helper-slot state 1 of the enemy, stepped by `field_49A`. Stage 0 waits out
/// the random delay in `field_4AA`; stage 1 moves to stage 4 once the player is
/// within 0x385 and inside +/-0x200 of the facing angle, otherwise picks a turn
/// direction; stages 2/3 turn the model by `field_484` (+/-25) on frames
/// 30..50 and re-check the angle every 60 frames; stages 4-6 play the
/// room-tagged sounds and toggle the display flags of two render nodes, stage 4
/// rolling a 40% chance of stage 6 before returning to stage 1.
void func_actor_207200_8014B87C(Task* arg0)
{
    Actor207200Work* work;
    GsCOORDINATE2*   coord;
    s32              angle;
    u32              dist;
    s32              id;
    s16              state;

    work  = (Actor207200Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->field_8;
    switch (work->field_49A) {
        case 0:
            work->field_48C = 1;
            if ((s16)work->field_490 > work->field_4AA) {
                work->field_49A = 1;
            }
            break;
        case 1:
            angle = ActorsShared80136614(((TmdObject*)arg0->extra)->field_8, &dist);
            if (work->field_4A6 == 0 && dist < 0x385 && ABS(angle) < 0x200) {
                work->field_492            = 0;
                work->field_49A            = 4;
                work->field_2C4.obj.flags &= 0xBFFF;
                break;
            }
            work->field_48C            = 2;
            work->field_498            = 0;
            work->field_2C4.obj.flags |= 0x4000;
            if ((u32)(work->field_490 - 20) < 20) {
                work->field_492 = D_actor_207200_80153F20[(s16)work->field_490 - 20];
            } else {
                work->field_492 = 0;
            }
            if ((s16)work->field_490 >= 75) {
                work->field_490 = 0;
                if (work->field_4A6 == 0) {
                    if (ABS(angle) > 0x200 || work->field_494 != 0) {
                        if (angle < 0) {
                            work->field_484 = -25;
                            work->field_49A = 3;
                            work->field_48C = 4;
                        } else {
                            work->field_484 = 25;
                            work->field_49A = 2;
                            work->field_48C = 3;
                        }
                    }
                }
            }
            break;
        case 2:
            state           = 3;
            work->field_492 = 0;
            work->field_48C = state;
            if ((u32)(work->field_490 - 30) < 21) {
                work->field_44C.vx  = 0;
                work->field_44C.vz  = 0;
                work->field_44C.vy += work->field_484;
                RotMatrix(&work->field_44C, &coord->coord);
            }
            if ((s16)work->field_490 >= 60) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    angle = ActorsShared80136614(((TmdObject*)arg0->extra)->field_8, &dist);
                    if (ABS(angle) < 0x200 || work->field_494 != 0) {
                        work->field_49A = 1;
                        work->field_494 = 0;
                        work->field_490 = 0;
                        work->field_48C = 2;
                    } else if (angle < 0) {
                        work->field_484 = -25;
                        work->field_490 = 0;
                        work->field_49A = 3;
                        work->field_48C = 4;
                    } else {
                        work->field_484 = 25;
                        work->field_490 = 0;
                        work->field_49A = 2;
                        work->field_48C = state;
                    }
                }
            }
            break;
        case 3:
            state           = 4;
            work->field_492 = 0;
            work->field_48C = state;
            if ((u32)(work->field_490 - 30) < 21) {
                work->field_44C.vx  = 0;
                work->field_44C.vz  = 0;
                work->field_44C.vy += work->field_484;
                RotMatrix(&work->field_44C, &coord->coord);
            }
            if ((s16)work->field_490 >= 60) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    angle = ActorsShared80136614(((TmdObject*)arg0->extra)->field_8, &dist);
                    if (ABS(angle) < 0x200 || work->field_494 != 0) {
                        work->field_49A = 1;
                        work->field_494 = 0;
                        work->field_490 = 0;
                        work->field_48C = 2;
                    } else if (angle < 0) {
                        work->field_484 = -25;
                        work->field_490 = 0;
                        work->field_49A = 3;
                        work->field_48C = state;
                    } else {
                        work->field_484 = 25;
                        work->field_49A = 2;
                        work->field_490 = 0;
                        work->field_48C = 3;
                    }
                }
            }
            break;
        case 4:
            work->field_492 = 0;
            if ((s16)work->field_490 == 30) {
                work->field_4A0 = 0;
                id              = ((((Actor207200Ctx*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x40480002;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if ((s16)work->field_490 == 42) {
                work->field_374.obj.flags |= 0x8000;
            }
            if ((s16)work->field_490 == 45) {
                work->field_374.obj.flags &= 0x7FFF;
            }
            if (work->field_4A0 != 0 && (s16)work->field_490 == 45) {
                id = ((((Actor207200Ctx*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x40480005;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_48C == 8 && (s16)work->field_490 >= 60) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((u16)((Gp_LcgState >> 16) % 100) < 40) {
                        work->field_49A = 6;
                        work->field_48C = 10;
                    } else {
                        work->field_49A = 1;
                        work->field_48C = 2;
                    }
                    work->field_490 = 0;
                }
            } else {
                work->field_48C = 8;
            }
            break;
        case 5:
            work->field_492 = 0;
            if ((s16)work->field_490 == 30) {
                work->field_3AC.obj.flags |= 0x8000;
            }
            if ((s16)work->field_490 == 60) {
                work->field_3AC.obj.flags &= 0x7FFF;
            }
            if ((s16)work->field_490 >= 90) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    work->field_49A = 1;
                    work->field_490 = 0;
                    work->field_48C = 2;
                }
            }
            break;
        case 6:
            work->field_48C = 10;
            work->field_492 = 0;
            if ((s16)work->field_490 == 10) {
                id = ((((Actor207200Ctx*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x40480006;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if ((s16)work->field_490 >= 90) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    work->field_49A = 1;
                    work->field_490 = 0;
                    work->field_48C = 2;
                }
            }
            break;
    }
}

/// Per-frame collision handling. Each six-record table's `func_800E0C10`
/// result pushes the model back (1) or snaps it to `field_454` (2). Records of
/// the first table then dispatch on their kind: 1 sets the turn state when the
/// player is off-angle and near, 2 applies damage from the player's distance,
/// 3 pushes the model out of the record's radius. Unless `field_4A6` is set,
/// each 0x20000 record of the second table applies damage too, and some ids
/// end the tick through `func_actor_207200_8014D128` / `8014CFEC`. The tables
/// and, when `field_49A` is set, the two part records are cleared last.
void func_actor_207200_8014BEF4(Task* arg0)
{
    Actor207200Work*       work;
    Actor207200DmgScratch* sc;
    Actor207200DmgScratch* head;
    GsCOORDINATE2*         coord;
    u32                    dist;
    GpEnemy*               enemy;
    s32                    i;
    s32                    angle;
    s32                    damage;
    s32                    push;
    s32                    param;
    s32                    n;
    s32                    snd;

    work                                     = (Actor207200Work*)arg0->work;
    head                                     = *(Actor207200DmgScratch**)G_SCRATCH_HEAD;
    *(Actor207200DmgScratch**)G_SCRATCH_HEAD = head - 1;
    sc                                       = head - 1;
    coord                                    = ((TmdObject*)arg0->extra)->field_8;
    enemy                                    = arg0->spawnArg2;

    switch (func_800E0C10((GpRec18*)work->field_2C4.field_20, &head[-1].d.delta, 6, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->d.delta.vx.h.hi;
            coord->coord.t[1] += sc->d.delta.vy.h.hi;
            coord->coord.t[2] += sc->d.delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_454;
            coord->coord.t[1] = work->field_458;
            coord->coord.t[2] = work->field_45C;
            if (work->field_4A6 == 0 && work->field_494 == 0) {
                work->field_494 = 1;
            }
            break;
    }
    switch (func_800E0C10((GpRec18*)work->field_214.field_20, &sc->d.delta, 6, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->d.delta.vx.h.hi;
            coord->coord.t[1] += sc->d.delta.vy.h.hi;
            coord->coord.t[2] += sc->d.delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_454;
            coord->coord.t[1] = work->field_458;
            coord->coord.t[2] = work->field_45C;
            if (work->field_4A6 == 0 && work->field_494 == 0) {
                work->field_494 = 1;
            }
            break;
    }
    if (work->field_49E != 0 && --work->field_49E <= 0) {
        work->field_49E = 0;
    }

    for (i = 0; i < 6; i++) {
        switch ((u32)((Actor207200HitView*)work)->rec2[i].hit.kind) {
            case 1:
                if (work->field_4A6 == 0 && (u16)work->field_49A - 1U < 3) {
                    angle = ActorsShared80136614(((TmdObject*)arg0->extra)->field_8, &dist);
                    if (abs(angle) > 0x200 && dist < 2000) {
                        work->field_48C = angle < 0 ? 7 : 6;
                        work->field_490 = 0;
                        work->field_49A = 5;
                    }
                }
                break;
            case 2:
                if (work->field_49E != 0) {
                    break;
                }
                sc->d.delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->d.delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->d.delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                damage           = SquareRoot0(sc->d.delta.vx.w * sc->d.delta.vx.w +
                                               sc->d.delta.vy.w * sc->d.delta.vy.w +
                                               sc->d.delta.vz.w * sc->d.delta.vz.w);
                Gp_GetIdParam0(((Actor207200HitView*)work)->rec2[i].rec.field_4);
                damage = Gp_ComputeDamage(((Actor207200HitView*)work)->rec2[i].rec.field_4, damage, 0, 0);
                func_800FDB18((u16)Gp_GetIdParam1(((Actor207200HitView*)work)->rec2[i].rec.field_4),
                              ((TmdObject*)arg0->extra)->field_8 + 1, &D_actor_207200_80153F10, &work->field_3EC);
                n = Gp_GetIdParam2(((Actor207200HitView*)work)->rec2[i].rec.field_4);
                if ((s16)n > 0) {
                    work->field_49E = n;
                }
                if (work->field_4A6 != 0 && arg0->killCountdown == 0) {
                    func_800DA6E8(&enemy->node, damage, 0);
                    if (damage != 0) {
                        arg0->state++;
                        work->field_48C                          = 0xB;
                        *(Actor207200DmgScratch**)G_SCRATCH_HEAD = *(Actor207200DmgScratch**)G_SCRATCH_HEAD + 1;
                        return;
                    }
                } else {
                    func_800DA6E8(&enemy->node, 0, 0);
                }
                if (work->field_486 == 0) {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x40480006;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                    work->field_48C = 5;
                    work->field_490 = 0;
                    work->field_486 = 4;
                    work->field_492 = 0;
                    work->field_4A2 = 0;
                }
                break;
            case 3:
                sc->d.delta.vx.w = coord->workm.t[0] - ((Actor207200HitView*)work)->rec2[i].hit.x;
                sc->d.delta.vy.w = 0;
                sc->d.delta.vz.w = coord->workm.t[2] - ((Actor207200HitView*)work)->rec2[i].hit.z;
                damage           = ((Actor207200HitView*)work)->rec2[i].hit.dist -
                         SquareRoot0(sc->d.delta.vx.w * sc->d.delta.vx.w + sc->d.delta.vz.w * sc->d.delta.vz.w);
                // Clamped through a second variable: clamping `damage` in
                // place drops the copy the original makes.
                push = damage;
                if (damage <= 0) {
                    push = 0;
                }
                damage           = push;
                sc->d.delta.vx.w = coord->workm.t[0] - ((Actor207200HitView*)work)->rec2[i].hit.x;
                sc->d.delta.vy.w = coord->workm.t[1] - ((Actor207200HitView*)work)->rec2[i].hit.y;
                sc->d.delta.vz.w = coord->workm.t[2] - ((Actor207200HitView*)work)->rec2[i].hit.z;
                VectorNormal(&sc->d.vec, &sc->norm);
                ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &sc->norm, &sc->d.vec);
                if (work->field_48C == 2) {
                    coord->coord.t[0] += (damage * sc->d.vec.vx) >> 12;
                    n                  = damage * sc->d.vec.vy;
                    if (n < 0) {
                        coord->coord.t[1] += n >> 12;
                    }
                    coord->coord.t[2] += (damage * sc->d.vec.vz) >> 12;
                }
                break;
        }
    }

    if (work->field_4A6 == 0) {
        for (i = 0; i < 6; i++) {
            if ((((Actor207200HitView*)work)->rec3[i].rec.field_4 & 0xFFFF0000) != 0x20000) {
                continue;
            }
            if (work->field_49E != 0) {
                break;
            }
            sc->d.delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->d.delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            sc->d.delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            damage           = SquareRoot0(sc->d.delta.vx.w * sc->d.delta.vx.w + sc->d.delta.vy.w * sc->d.delta.vy.w +
                                           sc->d.delta.vz.w * sc->d.delta.vz.w);
            param            = Gp_GetIdParam0(((Actor207200HitView*)work)->rec3[i].rec.field_4);
            damage           = Gp_ComputeDamage(((Actor207200HitView*)work)->rec3[i].rec.field_4, damage, 0, 0);
            switch ((u16)param) {
                case 1:
                case 4:
                case 5:
                case 6:
                    Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->field_8, 2, NULL);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    func_800DA6E8(&enemy->node, D_actor_207200_8014E7D8 * 2 + (u16)((Gp_LcgState >> 16) % 100), 0);
                    func_actor_207200_8014D128((Actor207200*)arg0);
                    work->field_4A8 = 1;
                    arg0->state++;
                    return;
                case 8:
                case 9:
                    Gp_SetObjFlag2((GpObj5D*)enemy, ((Actor207200HitView*)work)->rec3[i].rec.field_4, 0);
                default:
                    if ((Gp_RollEnemyChance(arg0->spawnArg2, ((Actor207200HitView*)work)->rec3[i].rec.field_4, 0) != 0 ||
                         work->field_486 == 3) &&
                        damage != 0) {
                        func_800E2C78((GpObj40*)enemy, ((Actor207200HitView*)work)->rec3[i].rec.field_4, damage, 0);
                        func_actor_207200_8014CFEC((Actor207200*)arg0);
                        return;
                    }
                    func_800E2C78((GpObj40*)enemy, ((Actor207200HitView*)work)->rec3[i].rec.field_4, damage, 0);
                    func_actor_207200_8014C870((Actor207200*)arg0, damage);
                    func_800FDB18((u16)Gp_GetIdParam1(((Actor207200HitView*)work)->rec3[i].rec.field_4),
                                  ((TmdObject*)arg0->extra)->field_8 + 3, &D_actor_207200_80153F08, &work->field_3E4);
                    n = Gp_GetIdParam2(((Actor207200HitView*)work)->rec3[i].rec.field_4);
                    if ((s16)n > 0) {
                        work->field_49E = n;
                    }
                    break;
            }
        }
    } else {
        work->field_374.obj.flags &= 0x7FFF;
        work->field_3AC.obj.flags &= 0x7FFF;
    }
    Gp_ClearRec18Occupied((GpRec18*)work->field_214.field_20);
    Gp_ClearRec18Occupied((GpRec18*)work->field_2C4.field_20);
    if (work->field_49A != 0) {
        if (Gp_FindRec18((GpRec18*)work->field_374.field_20, 0) != 0) {
            work->field_4A0            = 1;
            work->field_374.obj.flags &= 0x7FFF;
            Gp_ClearRec18Occupied((GpRec18*)work->field_374.field_20);
        }
        if (Gp_FindRec18((GpRec18*)work->field_3AC.field_20, 0) != 0) {
            work->field_3AC.obj.flags &= 0x7FFF;
            Gp_ClearRec18Occupied((GpRec18*)work->field_3AC.field_20);
        }
    }
    *(Actor207200DmgScratch**)G_SCRATCH_HEAD = *(Actor207200DmgScratch**)G_SCRATCH_HEAD + 1;
}

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
