#include "common.h"
#include <psyq/abs.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/gameplay.h"

#include "actors/actor.h"
#include "actors/actor_207200.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern GpU16Pair  D_actor_207200_8014E7CC;
extern GpPairSrcE D_actor_207200_8014E7D4;
extern s32        D_actor_207200_80153ED4;
/// `field_492` value for frames 20..39 of helper stage 1, indexed by frame - 20.
extern s16 D_actor_207200_80153F20[];
/// Base damage the shatter hit doubles, before a 0..99 roll is added.
extern u16 D_actor_207200_8014E7D8;
/// Effect offsets `func_800FDB18` is handed for the two hit tables.
extern SVECTOR D_actor_207200_80153F08;
extern SVECTOR D_actor_207200_80153F10;

/// The 0x4AC-byte allocation `func_actor_207200_8014B278` makes with
/// `memCalloc` and stores in `Task::work`: the animation context
/// `func_800B3F84` fills in, five `GpObj` render nodes with their `GpRec18`
/// tables (`Gp_LinkObj` shapes 3/2/2/3/3) and three `func_800FDB18` argument
/// records. `Actor207200Work` is the tick handlers' view of the same block and
/// does not yet agree with this layout over 0x234..0x2C4.
typedef struct Actor207200SpawnWork {
    /* 0x000 */ byte     pad_0[0x14];
    /* 0x014 */ byte     field_14[0x118];
    /* 0x12C */ byte     field_12C[0x70];
    /* 0x19C */ MATRIX   field_19C;
    /* 0x1BC */ MATRIX   field_1BC;
    /* 0x1DC */ GpObj    obj1;
    /* 0x1FC */ GpRec18  rec1[1];
    /* 0x214 */ GpObj    obj2;
    /* 0x234 */ GpRec18  rec2[6];
    /* 0x2C4 */ GpObj    obj3;
    /* 0x2E4 */ GpRec18  rec3[6];
    /* 0x374 */ GpObj    obj4;
    /* 0x394 */ GpRec18  rec4[1];
    /* 0x3AC */ GpObj    obj5;
    /* 0x3CC */ GpRec18  rec5[1];
    /* 0x3E4 */ GpEffArg eff0;
    /* 0x3EC */ GpEffArg eff1;
    /* 0x3F4 */ GpEffArg eff2;
    /* 0x3FC */ byte     pad_3FC[0x52];
    /* 0x44E */ u16      field_44E;
    /* 0x450 */ byte     pad_450[0x38];
    /* 0x488 */ s16      field_488;
    /* 0x48A */ byte     pad_48A[2];
    /* 0x48C */ s16      field_48C;
    /* 0x48E */ s16      field_48E;
    /* 0x490 */ byte     pad_490[4];
    /* 0x494 */ s16      field_494;
    /* 0x496 */ byte     pad_496[8];
    /* 0x49E */ s16      field_49E;
    /* 0x4A0 */ byte     pad_4A0[4];
    /* 0x4A4 */ s16      field_4A4;
    /* 0x4A6 */ s16      field_4A6;
    /* 0x4A8 */ s16      field_4A8;
} Actor207200SpawnWork;
STATIC_ASSERT_SIZEOF(Actor207200SpawnWork, 0x4AC);

/// 0x48-byte block `func_actor_207200_8014BEF4` takes from `G_SCRATCH_HEAD`:
/// `d` receives the `func_800E0C10` push-back, then the offset to the player
/// or to a push record, which `norm` holds normalised.
typedef struct Actor207200DmgScratch {
    /* 0x00 */ byte pad_0[0x20];
    /* 0x20 */ union {
        GpDeltaScratch delta;
        VECTOR         vec;
    } d;
    /* 0x30 */ byte   pad_30[8];
    /* 0x38 */ VECTOR norm;
} Actor207200DmgScratch;
STATIC_ASSERT_SIZEOF(Actor207200DmgScratch, 0x48);

/// A `GpRec18` collision record read either whole (`rec.key` is the hit
/// id) or split as a `GpHitRec` (`hit.kind` is the id's high half).
typedef union Actor207200HitRec {
    GpRec18  rec;
    GpHitRec hit;
} Actor207200HitRec;
STATIC_ASSERT_SIZEOF(Actor207200HitRec, 0x18);

/// `Actor207200Work` seen through the `Actor207200SpawnWork` layout over the
/// two six-record tables, which `Actor207200Work` does not yet describe. The
/// tables have to be struct members rather than casts of an address: the hit
/// loops only strength-reduce to a walker over `work` itself that way.
typedef struct Actor207200HitView {
    /* 0x000 */ byte              pad_0[0x234];
    /* 0x234 */ Actor207200HitRec rec2[6];
    /* 0x2C4 */ GpObj             obj3;
    /* 0x2E4 */ Actor207200HitRec rec3[6];
} Actor207200HitView;
STATIC_ASSERT_SIZEOF(Actor207200HitView, 0x374);

/// Effect-setup records handed to the spawned task through `D_80062730`.
extern u8      D_actor_207200_80150BCC[];
extern u8      D_actor_207200_80151074[];
extern u8      D_actor_207200_801517F8[];
extern SVECTOR D_actor_207200_80153F18;
/// Spawned task's setup argument (`D_800626EC[5].arg.model`).
extern s32 D_80062730;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void func_actor_207200_8014B278(GpEnemy* arg0, Task* arg1);
void func_actor_207200_8014C870(Task* arg0, s32 arg1);
s32  func_actor_207200_8014CE20(GsCOORDINATE2* arg0, u32* arg1);
void func_actor_207200_8014CA84(GpEnemy* arg0, Task* arg1);
void func_actor_207200_8014D2DC(GpEnemy* arg0, Task* arg1);
void func_actor_207200_8014CFEC(Task* arg0);
void func_actor_207200_8014D128(Task* arg0);
void func_actor_207200_8014D41C(Task* arg0);
void func_actor_207200_8014D49C(Task* arg0);
void func_actor_207200_8014D5C4(Task* arg0);
void func_actor_207200_8014D65C(Task* arg0);
void func_actor_207200_8014D70C(GpEnemy* arg0, Task* task);
void func_actor_207200_8014D77C(Task* task);
void func_actor_207200_8014D7E8(Task* arg0);
void func_actor_207200_8014D8DC(Task* arg0);
void func_actor_207200_8014D97C(Task* arg0, GsCOORDINATE2* arg1);
void func_actor_207200_8014DAF8(Task* dst, Task* src);
void func_actor_207200_8014DB4C(Task* arg0);

/// The large enemy's state handlers - spawn, live tick and teardown tick -
/// which `func_actor_207200_8014D280` dispatches through by task state.
const GpEnemyTaskFuncTable3 D_actor_207200_80149E30 = {
    { func_actor_207200_8014B278, func_actor_207200_8014D2DC, func_actor_207200_8014CA84 }
};

void func_actor_207200_8014B278(GpEnemy* arg0, Task* arg1)
{
    Actor207200SpawnWork* work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part6;
    GsCOORDINATE2*        part3;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    work  = memCalloc(0x4ACU, false);
    part6 = coord + 6;
    part3 = coord + 3;
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_1BC;
    obj->colorMtx  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = coord;
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &D_actor_207200_8014E7D4;
    arg0->recs       = work->rec3;
    arg0->hp         = (u16)D_actor_207200_8014E7D4.hpMax;
    work->field_44E  = ((GpCoordExt*)coord)->param.rot.vy;
    func_800B3F84((GpAnimCtx*)work, &D_actor_207200_80153ED4, obj,
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

    work->obj1.coord    = coord;
    work->obj1.ctx.recs = work->rec1;
    work->obj1.pos.vx   = 0;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0;
    work->obj1.key      = 0;
    work->obj1.radius   = 0x7D0;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 1, 0);

    work->obj2.pos.vy   = -0x12C;
    work->obj2.pos.vz   = -0xB4;
    work->obj2.coord    = coord;
    work->obj2.ctx.recs = work->rec2;
    work->obj2.pos.vx   = 0;
    work->obj2.key      = 0x3002B;
    work->obj2.radius   = 0x12C;
    work->obj2.flags    = 1;
    work->obj1.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->rec2, 6, 0);

    work->obj3.coord    = part3;
    work->obj3.ctx.recs = work->rec3;
    work->obj3.pos.vx   = 0;
    work->obj3.pos.vy   = 0;
    work->obj3.pos.vz   = 0;
    work->obj3.key      = 0x3002B;
    work->obj3.radius   = 0x96;
    work->obj3.flags    = 1;
    work->obj2.flags   |= 0xC200;
    Gp_LinkObj(2, &work->obj3);
    Gp_InitRec18Table(work->rec3, 6, 0);

    work->obj4.coord    = part3;
    work->obj4.ctx.recs = work->rec4;
    work->obj4.pos.vx   = 0;
    work->obj4.pos.vy   = 0x50;
    work->obj4.pos.vz   = 0x8C;
    work->obj3.flags   |= 0xC000;
    work->obj4.key      = Gp_PackPair(&D_actor_207200_8014E7CC, 0);
    work->obj4.radius   = 0x12C;
    work->obj4.flags    = 1;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec4, 1, 0);

    work->obj5.coord    = part6;
    work->obj5.ctx.recs = work->rec5;
    work->obj5.pos.vx   = 0xFA;
    work->obj5.pos.vy   = 0;
    work->obj5.pos.vz   = 0;
    work->obj4.flags   &= 0x7FFF;
    work->obj5.key      = Gp_PackPair(&D_actor_207200_8014E7CC, 1);
    work->obj5.radius   = 0x12C;
    work->obj5.flags    = 1;
    Gp_LinkObj(3, &work->obj5);
    Gp_InitRec18Table(work->rec5, 1, 0);
    work->obj5.flags &= 0x7FFF;

    work->eff0.coord      = ((TmdObject*)arg1->extra)->coords + 3;
    work->eff0.spawnArgLo = 0x100;
    work->eff0.spawnArgHi = 1;
    work->eff2.coord      = ((TmdObject*)arg1->extra)->coords + 3;
    work->eff2.spawnArgLo = 0x400;
    work->eff2.spawnArgHi = 3;
    work->eff1.coord      = ((TmdObject*)arg1->extra)->coords + 1;
    work->eff1.spawnArgLo = 0x100;
    work->eff1.spawnArgHi = 1;
    work->field_4A8       = 0;
    arg1->exitCallback    = (void (*)(Task*))func_actor_207200_8014DB4C;
    arg1->state++;
}

/// Helper-slot state 0 of the enemy: while it is still alive, a hit recorded in
/// the first render node's table (or the global flag `Gp_StateF0.field_3`) arms the
/// death sequence - helper state 1, a random 0..89 delay in `field_4AA` and
/// `Gp_ArmStateF0(1)`. Then runs the idle cycle in `field_48C`: state 1 waits
/// 0x5B frames and rolls a 30% chance of moving to 9, which plays the
/// room-tagged sound on frame 5 and returns to 1 after 0x2D frames.
void func_actor_207200_8014B628(Task* arg0)
{
    Actor207200Work* work;
    GsCOORDINATE2*   obj;
    s32              id;
    s32              pan;
    u32              rnd;
    u16              hi;

    *(u8**)G_SCRATCH_HEAD -= 8;
    work                   = (Actor207200Work*)arg0->work;
    obj                    = ((TmdObject*)arg0->extra)->coords;
    if (work->field_4A6 == 0) {
        if (Gp_CountRec18Hi((GpRec18*)work->field_1DC.field_20, 0x10000) != 0) {
            work->field_4A2 = 1;
        }
        if (work->field_4A2 != 0 || Gp_StateF0.field_3 != 0) {
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
                id  = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40480004;
                pan = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
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
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_49A) {
        case 0:
            work->field_48C = 1;
            if ((s16)work->field_490 > work->field_4AA) {
                work->field_49A = 1;
            }
            break;
        case 1:
            angle = func_actor_207200_8014CE20(((TmdObject*)arg0->extra)->coords, &dist);
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
                    angle = func_actor_207200_8014CE20(((TmdObject*)arg0->extra)->coords, &dist);
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
                    angle = func_actor_207200_8014CE20(((TmdObject*)arg0->extra)->coords, &dist);
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
                id              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40480002;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_490 == 42) {
                work->field_374.obj.flags |= 0x8000;
            }
            if ((s16)work->field_490 == 45) {
                work->field_374.obj.flags &= 0x7FFF;
            }
            if (work->field_4A0 != 0 && (s16)work->field_490 == 45) {
                id = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40480005;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
                id = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40480006;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
    coord                                    = ((TmdObject*)arg0->extra)->coords;
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
                    angle = func_actor_207200_8014CE20(((TmdObject*)arg0->extra)->coords, &dist);
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
                Gp_GetIdParam0(((Actor207200HitView*)work)->rec2[i].rec.key);
                damage = Gp_ComputeDamage(((Actor207200HitView*)work)->rec2[i].rec.key, damage, 0, 0);
                func_800FDB18((u16)Gp_GetIdParam1(((Actor207200HitView*)work)->rec2[i].rec.key),
                              ((TmdObject*)arg0->extra)->coords + 1, &D_actor_207200_80153F10, &work->field_3EC);
                n = Gp_GetIdParam2(((Actor207200HitView*)work)->rec2[i].rec.key);
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
                    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40480006;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
            if ((((Actor207200HitView*)work)->rec3[i].rec.key & 0xFFFF0000) != 0x20000) {
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
            param            = Gp_GetIdParam0(((Actor207200HitView*)work)->rec3[i].rec.key);
            damage           = Gp_ComputeDamage(((Actor207200HitView*)work)->rec3[i].rec.key, damage, 0, 0);
            switch ((u16)param) {
                case 1:
                case 4:
                case 5:
                case 6:
                    Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords, 2, NULL);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    func_800DA6E8(&enemy->node, D_actor_207200_8014E7D8 * 2 + (u16)((Gp_LcgState >> 16) % 100), 0);
                    func_actor_207200_8014D128(arg0);
                    work->field_4A8 = 1;
                    arg0->state++;
                    return;
                case 8:
                case 9:
                    Gp_SetObjFlag2(enemy, ((Actor207200HitView*)work)->rec3[i].rec.key, 0);
                default:
                    if ((Gp_RollEnemyChance(arg0->spawnArg2, ((Actor207200HitView*)work)->rec3[i].rec.key, 0) != 0 ||
                         work->field_486 == 3) &&
                        damage != 0) {
                        func_800E2C78(enemy, ((Actor207200HitView*)work)->rec3[i].rec.key, damage, 0);
                        func_actor_207200_8014CFEC(arg0);
                        return;
                    }
                    func_800E2C78(enemy, ((Actor207200HitView*)work)->rec3[i].rec.key, damage, 0);
                    func_actor_207200_8014C870(arg0, damage);
                    func_800FDB18((u16)Gp_GetIdParam1(((Actor207200HitView*)work)->rec3[i].rec.key),
                                  ((TmdObject*)arg0->extra)->coords + 3, &D_actor_207200_80153F08, &work->field_3E4);
                    n = Gp_GetIdParam2(((Actor207200HitView*)work)->rec3[i].rec.key);
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
void func_actor_207200_8014C870(Task* arg0, s32 arg1)
{
    Actor207200Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   coord;
    GpEffArg*        effArg;
    s32              snd;

    work  = arg0->work;
    ctx   = arg0->spawnArg2;
    coord = (*(TmdObject**)&arg0->extra)->coords;

    ctx->hp = (s16)((u16)ctx->hp - arg1);
    func_800DA6E8(&ctx->node, arg1, 0);
    if ((s16)ctx->hp <= 0) {
        if (work->field_4A6 == 0) {
            ctx->hp = 1;
            snd     = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40480003;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            effArg = &work->field_3F4;
            func_800FDB18(5, (*(TmdObject**)&arg0->extra)->coords + 3, &D_actor_207200_80153F18, effArg);
            func_800FDB18(5, (*(TmdObject**)&arg0->extra)->coords + 3, &D_actor_207200_80153F18, effArg);
            work->field_374.obj.flags &= 0x7FFF;
            work->field_3AC.obj.flags &= 0x7FFF;
            Gp_UnlinkObj(&work->field_2C4.obj);
            work->field_4A6     = 1;
            ctx->recs           = (GpRec18*)&work->field_214.field_20[0];
            work->field_488     = 0;
            arg0->killCountdown = 0x14;
        }
    } else {
        if (work->field_48C == 1) {
            snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40480006;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            work->field_48C = 5;
            work->field_490 = 0;
            work->field_486 = 4;
            work->field_492 = 0;
            work->field_4A2 = 0;
            return;
        }
        snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40480001;
        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
}

/// `func_actor_207200_8014D65C`'s body, inlined: re-arm the six helper slots
/// when the animation id changed, otherwise advance them by one frame.
static __inline__ void Actor207200_TickAnim(Task* arg0)
{
    Actor207200Work* work;
    s32              i;

    work = arg0->work;
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

/// `func_actor_207200_8014D70C`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
static __inline__ void Actor207200_UpdateColor(GpEnemy* enemy, Task* actor)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)actor->extra)->coords[1];
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

/// Teardown tick. Mode 2 of `Gp_StateF0.field_4` hides the model, mode 1 does nothing;
/// otherwise the teardown stage in `field_488` advances: 0 releases the actor's
/// state reference, snapshots the model transform and unlinks its node and
/// five display objects; 1 moves on once animation 5 has run 100 frames (or
/// at once for any other animation or once `field_4A8` is set); 2 counts 60
/// frames, spawning an effect on frame 15; 3 destroys the enemy. Every stage
/// but the last then ticks the animation, the attach coordinates and the colour.
void func_actor_207200_8014CA84(GpEnemy* arg0, Task* arg1)
{
    Actor207200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s16              state;

    obj   = (TmdObject*)arg1->extra;
    work  = arg1->work;
    coord = obj->coords;
    switch (Gp_StateF0.field_4) {
        case 1:
            break;
        case 2:
            obj->flags      |= 0x80;
            arg0->node.flags = 1;
            break;
        case 0:
        default:
            state = work->field_488;
            switch (state) {
                case 0:
                    Gp_ReleaseStateF0Add(arg1, 0x2B);
                    work->field_488 = 1;
                    work->field_48A = 0;
                    work->field_49C = 0x1000;
                    work->field_464 = coord->coord;
                    arg0->recs      = 0;
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
                        obj->flags      = 0x80;
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
                            obj->flags = 2;
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
            func_actor_207200_8014D97C(arg1, &((TmdObject*)arg1->extra)->coords[2]);
            func_actor_207200_8014D97C(arg1, &((TmdObject*)arg1->extra)->coords[3]);
            ((TmdObject*)arg1->extra)->coords[0].flg = 0;
            ((TmdObject*)arg1->extra)->coords[1].flg = 0;
            Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
            Actor207200_UpdateColor(arg0, arg1);
            break;
    }
}

/// Measures the model held in pointer slot 3 from coordinate `arg0`: returns
/// the heading to it in `arg0`'s own frame, folded into -0x800..0x800, and
/// stores the horizontal world distance in `*arg1`. The offset is staged in
/// 0x40 bytes of the scratch stack.
///
/// `base` and `vec` hold the same address on purpose: the explicit `move`
/// reproduces the original's `addiu`/`addu` pair, and the head-relative
/// spelling of the vector copy and of the reloaded x keeps the scratch
/// pointer in `head`'s register. `COMPILER_BARRIER` stops cse from forwarding
/// the reload of x from the store just above it.
s32 func_actor_207200_8014CE20(GsCOORDINATE2* arg0, u32* arg1)
{
    SVECTOR        local;
    GsCOORDINATE2* coord;
    s32            angle;
    s32            x;
    s16            z;
    void*          base;
    void*          head;
    void*          vec;
    void*          matrix;

    coord = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    head  = *(void**)0x1F8003FC;
    base  = head - 0x40;
    __asm__("move %0,%1" : "=r"(vec) : "r"(base));
    *(s16*)((s8*)base + 0) = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    *(s16*)((s8*)vec + 2)  = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    *(void**)0x1F8003FC    = vec;
    *(s16*)((s8*)vec + 4)  = (s16)(coord->workm.t[2] - arg0->workm.t[2]);
    matrix                 = head - 0x20;
    TransposeMatrix(&arg0->workm, matrix);
    local = *(SVECTOR*)((s8*)head - 0x40);
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0();
    gte_stsv(vec);
    angle = ratan2(*(s16*)((s8*)head - 0x40), *(s16*)((s8*)vec + 4));
    if (angle >= 0x801) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    *(s16*)((s8*)vec + 0) = (s16)(coord->coord.t[0] - arg0->coord.t[0]);
    COMPILER_BARRIER();
    x                     = *(s16*)((s8*)vec + 0);
    z                     = coord->coord.t[2] - arg0->coord.t[2];
    *(s16*)((s8*)vec + 4) = z;
    *arg1                 = SquareRoot0((x * x) + (z * z));
    *(void**)0x1F8003FC  += 0x40;
    return angle;
}

/// Spawns the pair of effects that carry this actor's death animation, hands
/// the spawned task `D_actor_207200_801517F8` as its setup argument, arms the
/// two timers on the work area and unlinks its third display object.
void func_actor_207200_8014CFEC(Task* arg0)
{
    GpEffArg*         effArg;
    struct GpEffWork* effect;
    Actor207200Work*  work;
    GpEnemy*          ctx;

    work = arg0->work;
    ctx  = arg0->spawnArg2;

    Gp_SpawnEff(0x6009C, (*(TmdObject**)&arg0->extra)->coords, 0, NULL);
    func_800DA6E8(&ctx->node, ctx->hp - 1, 0);
    D_80062730 = (s32)&D_actor_207200_801517F8;
    effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 3, 0, NULL);
    if (effect != NULL) {
        func_actor_207200_8014DAF8(effect->task, arg0);
    }
    effArg = &work->field_3F4;
    func_800FDB18(5, (*(TmdObject**)&arg0->extra)->coords + 3, &D_actor_207200_80153F18, effArg);
    func_800FDB18(5, (*(TmdObject**)&arg0->extra)->coords + 3, &D_actor_207200_80153F18, effArg);
    work->field_4A4            = 1;
    work->field_48C            = 5;
    work->field_4A6            = 1;
    ctx->recs                  = (GpRec18*)&work->field_214.field_20[0];
    ctx->hp                    = 1;
    work->field_2C4.obj.flags &= 0x7FFF;
    Gp_UnlinkObj(&work->field_2C4.obj);
    arg0->killCountdown = 0x14;
}

void func_actor_207200_8014D128(Task* arg0)
{
    GpEffWork* effect;
    s32        r;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    r           = (Gp_LcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_80062730 = (s32)&D_actor_207200_801517F8;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 3, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
        case 2:
            D_80062730 = (s32)&D_actor_207200_80151074;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 5, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
        case 3:
            D_80062730 = (s32)&D_actor_207200_80150BCC;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 2, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(0x60030, (*(TmdObject**)&arg0->extra)->coords + 3, 0x300, NULL);
    Gp_SpawnEff(0x60030, (*(TmdObject**)&arg0->extra)->coords + 2, 0x300, NULL);
}

void func_actor_207200_8014D280(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame tick of the actor's live state. `Gp_StateF0.field_4` gates it: mode 1
/// skips the update and runs only the tail, mode 2 puts the model in its
/// hidden pose (part flag 0x80, node flag 1) and returns without updating,
/// mode 0 clears both flags before falling into the update, and any other mode
/// updates directly. The update drives the model's two attach coordinates,
/// clears the display flags of the first two parts and recomputes the second
/// part's world matrix; the tail then colours the actor from that part and
/// draws its ground shadow.
void func_actor_207200_8014D2DC(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = Gp_StateF0.field_4;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    ((TmdObject*)arg1->extra)->flags = 0;
    arg0->node.flags                 = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.flags                 = one;
    return;
default_body:
    func_actor_207200_8014D41C(arg1);
    func_actor_207200_8014D8DC(arg1);
    func_actor_207200_8014BEF4(arg1);
    func_actor_207200_8014D49C(arg1);
    func_actor_207200_8014D5C4(arg1);
    func_actor_207200_8014D65C(arg1);
    func_actor_207200_8014D97C(arg1, &((TmdObject*)arg1->extra)->coords[2]);
    func_actor_207200_8014D97C(arg1, &((TmdObject*)arg1->extra)->coords[3]);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    func_actor_207200_8014D70C(arg0, arg1);
    func_actor_207200_8014D77C(arg1);
}

/// Consumes the pending flag bits on the actor's spawn object once the actor
/// has been set up. Bit 0x1 (the "flag 1" request) is cleared first; bit 0x2
/// then re-arms the six helper slots - back to state 3 with slot id 1 at weight
/// 9 and every frame counter reset - and clears itself; bits 0xC (the "flag 4"
/// request) are cleared last. Nothing happens while the whole byte is zero.
void func_actor_207200_8014D41C(Task* arg0)
{
    GpEnemy*         obj;
    Actor207200Work* work;
    u8               flags;

    obj   = arg0->spawnArg2;
    flags = obj->reactionFlags;
    work  = arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            obj->reactionFlags = flags & 0xFE;
        }
        if (obj->reactionFlags & 2) {
            obj->reactionFlags = obj->reactionFlags & 0xFD;
            work->field_486    = 3;
            work->field_48E    = 1;
            work->field_48A    = 0;
            work->field_492    = 0;
            work->field_48C    = 9;
            work->field_490    = 0;
        }
        flags = obj->reactionFlags;
        if (flags & 0xC) {
            obj->reactionFlags = flags & 0xF3;
        }
    }
}

/// Per-frame tick of the actor's six helper slots, driven by
/// `work->field_486`. The kill countdown on the task is decremented first and
/// clamped at zero. State 0 and state 1 hand the actor to the two helper
/// setup/tick bodies; state 3 runs the slot animation, counting
/// `work->field_48A` up to 0x3D frames before re-arming the slots with id 1 at
/// weight 9, and drops back to state 0 once `Gp_TickObjFlag2` reports that the
/// spawn argument is done; state 4 waits out `work->field_490` frames and then
/// either returns to state 1 when the actor is idle (`work->field_4A6 != 0`)
/// or clears both state words and marks `work->field_4A2`.
void func_actor_207200_8014D49C(Task* arg0)
{
    Actor207200Work* work;
    s16              countdown;

    work                = arg0->work;
    countdown           = (u16)arg0->killCountdown - 1;
    arg0->killCountdown = countdown;
    if (countdown < 0) {
        arg0->killCountdown = 0;
    }
    switch (work->field_486) {
        case 0:
            func_actor_207200_8014B628(arg0);
            break;
        case 1:
            func_actor_207200_8014B87C(arg0);
            break;
        case 3:
            work->field_48A = work->field_48A + 1;
            if ((s16)work->field_48A >= 0x3D) {
                work->field_48E = 1;
                work->field_48C = 9;
                work->field_490 = 0;
                work->field_48A = 0;
            }
            if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
                work->field_486 = 0;
            }
            break;
        case 4:
            if ((s16)work->field_490 >= 0x69) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                    break;
                }
                work->field_486 = 0;
                work->field_4A2 = 1;
            }
            break;
    }
}

/// Walks the model's root part forward. While the actor is not idle
/// (`work->field_4A6 == 0`) the part's current translation is remembered in the
/// work area, and the part is then displaced along its own forward axis - the
/// third basis column of its local matrix, scaled by `work->field_492` - and
/// lifted by 0x80.
void func_actor_207200_8014D5C4(Task* arg0)
{
    Actor207200Work* work;
    GsCOORDINATE2*   coord;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_4A6 == 0) {
        work->field_454 = coord->coord.t[0];
        work->field_458 = coord->coord.t[1];
        work->field_45C = coord->coord.t[2];
    }
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_492) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_492) >> 12;
}

/// Rebinds the work's animation id to its six helper slots. When the id has
/// changed since the last frame the remembered id follows it, the frame counter
/// restarts and every slot is pointed at the new id at weight 8; otherwise the
/// counter ticks and the slots are simply advanced by one.
void func_actor_207200_8014D65C(Task* arg0)
{
    Actor207200Work* work;
    s32              i;

    work = arg0->work;
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

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_207200_8014D70C(GpEnemy* arg0, Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Draws the enemy's ground quad under its model root, at the translation of
/// the root part's `workm`, staged in a `VECTOR3` on the scratch stack.
void func_actor_207200_8014D77C(Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    coord   = ((TmdObject*)task->extra)->coords;
    vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(vec, 0x1C0, 0);
    SCRATCH_SP += 0x18;
}

/// Rebuilds the first coordinate node of the actor's model from the transform
/// stored in `work->field_464`, scaled along Y by `work->field_49C` (its own
/// angle field, decaying by 0x50 a frame while it sits above 0x200). The 0x30
/// bytes that hold the scaling matrix and its `VECTOR` are borrowed from the
/// scratchpad and released again; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void func_actor_207200_8014D7E8(Task* arg0)
{
    GsCOORDINATE2*     coord;
    MATRIX*            head;
    ActorScaleScratch* scratch;
    Actor207200Work*   work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorScaleScratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->extra)->coords;
    if (work->field_49C >= 0x201) {
        work->field_49C = (u16)work->field_49C - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_49C;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_464;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// Re-picks the model part the enemy's `coord` points at and relinks its
/// lock-on node. Once `field_4A6` is set it is always the second part;
/// before that it is the fourth part while the model in pointer slot 3 lies
/// within a quarter turn of the root's heading, and the second otherwise.
void func_actor_207200_8014D8DC(Task* arg0)
{
    Actor207200Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   coord;
    s32              dist;
    s32              angle;

    work = arg0->work;
    ctx  = arg0->spawnArg2;
    if (work->field_4A6 != 0) {
        coord = (*(TmdObject**)&arg0->extra)->coords + 1;
    } else {
        angle = func_actor_207200_8014CE20((*(TmdObject**)&arg0->extra)->coords, &dist);
        if (angle < 0) {
            angle = -angle;
        }
        if (angle < 0x400) {
            coord = (*(TmdObject**)&arg0->extra)->coords + 3;
        } else {
            coord = (*(TmdObject**)&arg0->extra)->coords + 1;
        }
    }
    ctx->coord = coord;
    Gp_LinkNode(&ctx->node);
}

/// Reads the matrix column at offsets `o0` / `o1` / `o2` of `r0` into the
/// `SVECTOR` at `r1`.
#define ACTOR_COPY_MATRIX_COLUMN_TO_SV(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, %2(%0);"                                 \
        "lhu $13, %3(%0);"                                 \
        "lhu $14, %4(%0);"                                 \
        "sh $12, 0(%1);"                                   \
        "sh $13, 2(%1);"                                   \
        "sh $14, 4(%1)"                                    \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

/// The way back: the `SVECTOR` at `r0` into the column of `r1`.
#define ACTOR_COPY_SV_TO_MATRIX_COLUMN(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, 0(%0);"                                  \
        "lhu $13, 2(%0);"                                  \
        "lhu $14, 4(%0);"                                  \
        "sh $12, %2(%1);"                                  \
        "sh $13, %3(%1);"                                  \
        "sh $14, %4(%1)"                                   \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

/// While `work->field_4A6` is set, runs each column of the node's rotation
/// matrix through GTE `gpf 12` with a zero interpolation factor, zeroing the
/// 3x3 part, and clears `flg` so the node is recomputed.
void func_actor_207200_8014D97C(Task* arg0, GsCOORDINATE2* arg1)
{
    SVECTOR vec;
    MATRIX* m;

    if (((Actor207200Work*)arg0->work)->field_4A6 != 0) {
        m = &arg1->coord;
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 0, 6, 12);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 0, 6, 12);

        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 2, 8, 14);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 2, 8, 14);

        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 4, 10, 16);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 4, 10, 16);

        arg1->flg = 0;
    }
}

/// Copies the texture page and CLUT from `src`'s model onto `dst`'s and, when
/// `dst` has a stream buffer, processes it twice so both halves pick the new
/// pair up. The enemy calls it with a freshly spawned effect as `dst` and
/// itself as `src`.
void func_actor_207200_8014DAF8(Task* dst, Task* src)
{
    TmdObject* to;
    TmdObject* from;

    from      = (TmdObject*)src->extra;
    to        = (TmdObject*)dst->extra;
    to->tpage = from->tpage;
    to->clut  = from->clut;
    if (to->buffer != NULL) {
        tmdProcessStream(to);
        tmdProcessStream(to);
    }
}

void func_actor_207200_8014DB4C(Task* arg0)
{
    GpEnemy*         ctx;
    Actor207200Work* work;

    ctx       = arg0->spawnArg2;
    work      = arg0->work;
    ctx->recs = 0;
    Gp_UnlinkNode(&ctx->node);
    Gp_UnlinkObj(&work->field_1DC.obj);
    Gp_UnlinkObj(&work->field_214.obj);
    Gp_UnlinkObj(&work->field_2C4.obj);
    Gp_UnlinkObj(&work->field_374.obj);
    Gp_UnlinkObj(&work->field_3AC.obj);
    Gp_EnemyTaskExit(arg0);
}
