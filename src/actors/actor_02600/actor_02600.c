#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_80135c4c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Work block of the actor task. Its prefix - a linked `GpObj`, one 0x18-byte
/// collision record, `field_38` and `field_3A` - is also how
/// `Actor02600_Fn02780` reads the projectile task's 0x40-byte work.
typedef struct Actor202600Work {
    /* 0x000 */ GpObj     obj;
    /* 0x020 */ GpRec18   rec;
    /* 0x038 */ s16       field_38;
    /* 0x03A */ s16       field_3A;
    /* 0x03C */ byte      pad_3C[0x118];
    /* 0x154 */ byte      field_154[0x80];
    /* 0x1D4 */ MATRIX    field_1D4;
    /* 0x1F4 */ MATRIX    field_1F4;
    /* 0x214 */ GpObj     field_214;
    /* 0x234 */ GpRec18   field_234[4];
    /* 0x294 */ GpObj     field_294;
    /* 0x2B4 */ GpRec18   field_2B4[2];
    /* 0x2E4 */ GpObj     field_2E4;
    /* 0x304 */ GpRec18   field_304[1];
    /* 0x31C */ GpObj     field_31C;
    /* 0x33C */ GpRec18   field_33C[1];
    /* 0x354 */ GpEffArg  field_354;
    /* 0x35C */ VECTOR3   field_35C;
    /* 0x368 */ byte      pad_368[4];
    /* 0x36C */ TaskDesc* field_36C;
    /* 0x370 */ MATRIX    field_370;
    /* 0x390 */ s16       field_390;
    /* 0x392 */ s16       field_392;
    /* 0x394 */ s16       field_394;
    /* 0x396 */ u16       field_396;
    /* 0x398 */ s16       field_398;
    /* 0x39A */ s16       field_39A;
    /* 0x39C */ s16       field_39C;
    /* 0x39E */ s16       field_39E;
    /* 0x3A0 */ s16       field_3A0;
    /* 0x3A2 */ s16       field_3A2;
    /* 0x3A4 */ s16       field_3A4;
    /* 0x3A6 */ s16       field_3A6;
    /* 0x3A8 */ s16       field_3A8;
    /* 0x3AA */ s16       field_3AA;
    /* 0x3AC */ u16       field_3AC;
    /* 0x3AE */ byte      pad_3AE[2];
    /* 0x3B0 */ s16       field_3B0;
    /* 0x3B2 */ s16       field_3B2;
    /* 0x3B4 */ s16       field_3B4;
    /* 0x3B6 */ s16       field_3B6;
    /* 0x3B8 */ byte      pad_3B8[2];
    /* 0x3BA */ s16       field_3BA;
    /* 0x3BC */ s16       field_3BC;
    /* 0x3BE */ s16       field_3BE;
    /* 0x3C0 */ s16       field_3C0;
    /* 0x3C2 */ s16       field_3C2;
    /* 0x3C4 */ s16       field_3C4;
    /* 0x3C6 */ s16       field_3C6;
    /* 0x3C8 */ s16       field_3C8;
    /* 0x3CA */ s16       field_3CA;
    /* 0x3CC */ s16       field_3CC;
    /* 0x3CE */ s16       field_3CE;
    /* 0x3D0 */ s16       field_3D0;
    /* 0x3D2 */ s16       field_3D2;
} Actor202600Work;
STATIC_ASSERT_SIZEOF(Actor202600Work, 0x3D4);

/// The 0x28-byte scratch `Actor02600_Fn02954` projects through
/// `GsWSMATRIX`: `p[0]` holds the source position going in and the four
/// projected corners coming out, `screen` is the single `SXY2` those come from
/// and `depth` the `SZ3` that gates the draw.
typedef struct Actor202600QuadScratch {
    /* 0x00 */ SVECTOR p[4];
    /* 0x20 */ s32     screen;
    /* 0x24 */ s32     depth;
} Actor202600QuadScratch;
STATIC_ASSERT_SIZEOF(Actor202600QuadScratch, 0x28);

/// One frame's 0x20x0x20 texture window inside the sprite atlas.
typedef struct Actor202600Uv {
    /* 0x00 */ u8 u;
    /* 0x01 */ u8 pad_1;
    /* 0x02 */ u8 v;
    /* 0x03 */ u8 pad_3;
} Actor202600Uv;

extern Actor202600Uv Actor02600_D08A78[];
extern s16           Actor02600_D08A98[];

/// The 0x38-byte scratch a push-back needs: `delta` is filled by the collision
/// walk, `unit` is its normal and `local` the same vector in the grid's space;
/// `rot` is the temporary `RotMatrix` source and `func_800FDB18` effect angle.
typedef struct Actor202600HitScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ VECTOR         unit;
    /* 0x20 */ VECTOR         local;
    /* 0x30 */ SVECTOR        rot;
} Actor202600HitScratch;
STATIC_ASSERT_SIZEOF(Actor202600HitScratch, 0x38);

/// Animation view of the work prefix: the 0x14-byte context `func_800B3F84`
/// fills in, followed by the eight slots it is handed and `Gp_AnimResetSlot`
/// walks.
typedef struct Actor202600Anim {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[8];
} Actor202600Anim;

typedef struct Actor202600RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor202600RotScratch;
STATIC_ASSERT_SIZEOF(Actor202600RotScratch, 0x18);

extern GpPairSrcE Actor02600_D08968;
extern SVECTOR    Actor02600_D089B0[];
extern s16        Actor02600_D089D0[];
extern SVECTOR    Actor02600_D089E8[];
extern s16        Actor02600_D08A08[];
extern TaskDesc   Actor02600_D08AB4;
extern u8         Actor02600_D08ACC[];
extern u16        Actor02600_D08978[];
extern u16        Actor02600_D08988[];
extern u16        Actor02600_D08998[];
extern s16        Actor02600_D089A8[];
extern s16        Actor02600_D089D8[];
extern s16        Actor02600_D089E0[];
extern s16        Actor02600_D08A16;
extern s16        Actor02600_D08A18;
extern s16        Actor02600_D08A30[][2];
extern s16        Actor02600_D08A54[][2];
extern u8         D_801153F2[2];
extern s8         D_8011540E;
extern s8         D_80115412;
extern MATRIX*    D_80073B8C[1];
extern GpU16Pair  Actor02600_D08950;

/// Scratchpad block the line draw takes below the scratchpad top: `position`
/// is the point handed to the GTE, `screen` and `depth` its projection. The
/// first 0x10 bytes are reserved but never touched.
typedef struct Actor202600LineScratch {
    /* 0x00 */ s32     unused[4];
    /* 0x10 */ SVECTOR position;
    /* 0x18 */ s32     screen;
    /* 0x1C */ s32     depth;
} Actor202600LineScratch;
STATIC_ASSERT_SIZEOF(Actor202600LineScratch, 0x20);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/* Animation id -> slot blend value table in this overlay's own data. */
extern s16 Actor02600_D08A10[];

/* `D_80067704` is the third word of a `D_800676A8` record: it selects the model
 * stream the next `Gp_SpawnEff` uses for the effect's own `TmdObject`. Declared
 * as a one-element array so GCC 2.8.1 cannot treat the store as non-aliasing
 * with the struct traffic that follows and sink it past the loads. */
extern void* D_80067704[1];

/* Model stream in this overlay's own data. */
extern u8 Actor02600_D05F10[];

void Actor02600_Fn02364(GpEnemy* arg0, Task* arg1);
void Actor02600_Fn02780(GpEnemy* arg0, Task* arg1);
void Actor02600_Fn02954(Task* actor, s32 frame);
void Actor02600_Fn02C94(Task* actor);
void Actor02600_Fn02FFC(GpEnemy* ctx, Task* actor);
void Actor02600_Fn03558(GpEnemy* arg0, Task* arg1);
void Actor02600_Fn0366C(Task* arg0);
void Actor02600_Fn03784(Task* arg0);
void Actor02600_Fn0385C(Task* arg0);
void Actor02600_Fn03910(Task* arg0);
void Actor02600_Fn039A4(Task* arg0);
void Actor02600_Fn03A68(Task* arg0);
void Actor02600_Fn03AC0(Task* arg0);
void Actor02600_Fn03B58(Task* arg0);
void Actor02600_Fn03C4C(Task* actor);
void Actor02600_Fn03D38(Task* actor);
void Actor02600_Fn03E2C(GpEnemy* enemy, Task* task);

void Actor02600_Fn0006C(Task* arg0)
{
    Actor202600Work*       work;
    Actor202600HitScratch* head;
    Actor202600HitScratch* s;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         src;
    s32                    movement;
    s32                    lastId;
    u32                    damage;
    s16                    amount;
    s32                    best;
    s32                    push;
    s32                    val;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    s32                    z;
    s32                    wallDx;
    s32                    wallDy;
    s32                    wallDz;
    VECTOR*                unit;
    s32                    i;
    s16                    timer;
    s32                    one;
    u32                    kind;

    best   = 0;
    lastId = 0;
    work   = arg0->work;
    coord  = ((TmdObject*)arg0->extra)->coords;
    head   = *(Actor202600HitScratch**)PSX_SCRATCH_ADDR(0x3FC);
    s = *(Actor202600HitScratch**)PSX_SCRATCH_ADDR(0x3FC) = head - 1;
    enemy                                                 = (GpEnemy*)arg0->spawnArg2;
    work->field_3CC                                       = 0;
    movement                                              = func_800E0C10(work->field_234, &s->delta, 4, NULL);
    if (movement != 0) {
        if (work->field_39A == 2) {
            work->field_3CC = 1;
        }
        switch (movement) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += head[-1].delta.vx.h.hi;
                coord->coord.t[1] += s->delta.vy.h.hi;
                z                  = coord->coord.t[2] + s->delta.vz.h.hi;
                coord->coord.t[2]  = z;
                break;
            case 2:
                coord->coord.t[0] = work->field_35C.vx;
                coord->coord.t[1] = work->field_35C.vy;
                coord->coord.t[2] = work->field_35C.vz;
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_234);
    if (work->field_390 != 0) {
        timer           = (u16)work->field_390 - 1;
        work->field_390 = timer;
        if (timer <= 0) {
            work->field_390 = 0;
        }
    }
    one = 1;
    TOUCH_REG_USE(movement, s);
    work->field_3D0 = 0;
    work->field_3BA = 0;
    unit            = &s->unit;
    for (i = 0; i < 2; i++) {
        kind = (u32)work->field_2B4[i].key >> 16;
        if (kind == one)
            goto physical;
        if (kind == 0)
            goto next_contact;
        if (kind == 2)
            goto damage_contact;
        if (kind == 3)
            goto physical;
        goto next_contact;
    damage_contact:
        if (work->field_390 == 0) {
            movement = 0;
            if ((((u32)work->field_2B4[i].key >> 8) & 0x3F) == 0x24) {
                if ((work->field_2B4[i].key & 0x3F) == 0x24) {
                    movement = 1;
                }
            }
            if ((movement != one) || (work->field_3B2 == 0)) {
                src           = ((TmdObject*)Gp_ActorSlots[((u32)work->field_2B4[i].key >> 7) & 1]->extra)->coords;
                dx            = src->coord.t[0] - coord->coord.t[0];
                s->delta.vx.w = dx;
                dy            = src->coord.t[1] - coord->coord.t[1];
                s->delta.vy.w = dy;
                dz            = src->coord.t[2] - coord->coord.t[2];
                s->delta.vz.w = dz;
                damage        = Gp_ComputeDamage((u32)work->field_2B4[i].key, SquareRoot0(dx * dx + dy * dy + dz * dz), 0, 0);
                amount        = damage;
                if (movement == 0) {
                    if (work->field_3CA != 0) {
                        amount = (damage << 16) >> 15;
                        Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords + 1, 3, NULL);
                    }
                    if (Gp_RollEnemyChance(enemy, (u32)work->field_2B4[i].key, 0) != 0) {
                        amount = (amount << 16) >> 14;
                        if (work->field_3CA == 0) {
                            Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords + 1, 0, NULL);
                        }
                    }
                    func_800E2C78(enemy, (u32)work->field_2B4[i].key, amount, 0);
                }
                func_800DA6E8(&enemy->node, amount, 0);
                enemy->hp -= amount;
                if (work->field_3C8 != one) {
                    if (enemy->hp <= 0) {
                        work->field_39A = 9;
                        work->field_39C = 0;
                        arg0->state     = 2;
                    } else if (movement == 0) {
                        work->field_39A = 6;
                        work->field_39C = 0;
                    }
                }
                if (work->field_3C8 == 2) {
                    if ((work->field_39A == 9) || (movement == 0)) {
                        do {
                            work->field_3C8 = 0;
                            work->field_3A2 = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                            s->rot.vx       = 0;
                            s->rot.vy       = (u16)work->field_3A2 + 0x800;
                            s->rot.vz       = 0;
                            RotMatrix(&s->rot, &coord->coord);
                        } while (0);
                    }
                }
                if (movement == 0) {
                    work->field_3D0        = one;
                    work->field_2E4.flags &= 0x3FFF;
                }
                switch (Gp_GetIdParam0(work->field_2B4[i].key) & 0xFFFF) {
                    case 0:
                    case 1:
                    case 5:
                    case 8:
                    case 9:
                        break;
                    case 2:
                        Gp_SetObjFlag2(enemy, work->field_2B4[i].key, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(enemy, work->field_2B4[i].key, 0);
                        break;
                    case 4:
                    case 6:
                        if (work->field_3C8 != one) {
                            work->field_3BA = one;
                        }
                        break;
                    case 7:
                        if (work->field_3B0 == 0) {
                            work->field_3B0        = one;
                            work->field_3BE        = 0;
                            work->field_3B2        = 0;
                            work->field_31C.flags |= 0x8000;
                            Gp_SetLightMode(arg0->spawnArg2, 3);
                        }
                        break;
                }
                if (lastId != work->field_2B4[i].key) {
                    lastId    = work->field_2B4[i].key;
                    s->rot.vx = 0;
                    s->rot.vy = -0xC8;
                    s->rot.vz = 0;
                    func_800FDB18(Gp_GetIdParam1(work->field_2B4[i].key) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 1, &s->rot, &work->field_354);
                }
                movement = Gp_GetIdParam2(work->field_2B4[i].key);
                if (movement > 0) {
                    work->field_390 = movement;
                }
            }
        }
        goto next_contact;
    physical:
        wallDx        = coord->workm.t[0] - work->field_2B4[i].point.vx;
        s->delta.vx.w = wallDx;
        wallDy        = coord->workm.t[1] - work->field_2B4[i].point.vy;
        s->delta.vy.w = wallDy;
        wallDz        = coord->workm.t[2] - work->field_2B4[i].point.vz;
        s->delta.vz.w = wallDz;
        push          = work->field_2B4[i].depth - SquareRoot0(wallDx * wallDx + wallDy * wallDy + wallDz * wallDz);
        val           = push;
        if (push <= 0) {
            val = 0;
        }
        push = val;
        if (best < push) {
            best = push;
            VectorNormal((VECTOR*)&s->delta, unit);
            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, unit, &s->local);
        }
    next_contact:;
    }
    if (best > 0) {
        coord->coord.t[0] += (best * s->local.vx) >> 12;
        coord->coord.t[2] += (best * s->local.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->field_2B4);
    work->field_3CE = 0;
    if (work->field_304[0].flags & 1) {
        if (((work->field_304[0].key & 0xFFFF0000) == 0x10000) ||
            (((work->field_304[0].key & 0xFFFF0000) == 0x100000) && (work->field_304[0].at10.normal.vy == 0))) {
            do {
                work->field_3CE = 1;
            } while (0);
        }
        work->field_2E4.flags &= 0x3FFF;
        Gp_ClearRec18Occupied(work->field_304);
    }
    *(Actor202600HitScratch**)PSX_SCRATCH_ADDR(0x3FC) = *(Actor202600HitScratch**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

/// State handlers of the projectile task `Actor02600_Fn03DD0` dispatches,
/// indexed by `Task::state`: setup, per-frame tick and `Gp_DestroyEnemy`.
const GpEnemyTaskFuncTable3 Actor02600_D0002C = {
    {
        Actor02600_Fn03E2C,
        Actor02600_Fn02780,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the actor task `Actor02600_Fn03F80` dispatches, indexed
/// by `Task::state`: spawn, per-frame tick and the dying sequence.
const GpEnemyTaskFuncTable3 Actor02600_D00038 = {
    {
        Actor02600_Fn02FFC,
        Actor02600_Fn03558,
        Actor02600_Fn02364,
    },
};

void Actor02600_Fn00754(Task* arg0)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              dx;
    s32              dz;
    u32              random;
    s32              index;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = arg0->work;
    state                              = work->field_39C;
    coord                              = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            delta->vy         = 0;
            dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            delta->vz         = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x9C4) || (work->field_3D0 != 0) || (D_801153F2[1] == 2)) {
                work->field_39C = 1;
                work->field_392 = 0xD;
                Gp_ArmStateF0(1);
            }
            break;
        case 1:
            if ((u32)(work->field_396 - 0xB) < 0x32U) {
                coord->coord.t[2] += 4;
            }
            if ((s16)work->field_396 >= 0x4B) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = state;
                index           = ((GpEnemy*)arg0->spawnArg2)->place->rowIndex;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = Actor02600_D08978[index] + ((random >> 0x10) & 0xF);
            }
            break;
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void Actor02600_Fn00914(Task* arg0)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s16              angle;
    s32              magnitude;
    s16              wrapped;
    s16              difference;
    s32              distance;
    s32              dx;
    s32              dz;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    coord                              = ((TmdObject*)arg0->extra)->coords;
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = arg0->work;
    work->field_3A2                    = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
    scratchEnd[-1].vx                  = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy                          = 0;
    dz                                 = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    delta->vz                          = dz;
    dx                                 = scratchEnd[-1].vx;
    distance                           = SquareRoot0((dx * dx) + (dz * dz));
    angle                              = (u16)work->field_3A2 - (ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)delta->vz) & 0xFFF);
    magnitude                          = __builtin_abs((s32)angle);
    if (magnitude < 0x800) {
        difference = magnitude;
    } else {
        if (angle > 0) {
            wrapped = 0x1000 - angle;
        } else {
            wrapped = angle + 0x1000;
        }
        difference = wrapped;
    }
    if ((distance < 0x9C4) && (difference < 0x80)) {
        work->field_39A = 5;
        work->field_39C = 0;
        work->field_392 = 4;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void Actor02600_Fn00A94(Task* arg0)
{
    register Task*   actor asm("s5") = arg0;
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              pan0;
    s32              pan1;
    s32              pan2;
    s32              sessionFlags;
    s32              dx;
    s32              dz;
    s32              value;
    u32              random;
    s32              index;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    SOFT_TOUCH_REG(actor);
    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = actor->work;
    coord                              = ((TmdObject*)actor->extra)->coords;
    state                              = work->field_39C;
    sessionFlags                       = *(s32*)&gGameSession->at4.loc.view;
    value                              = 0;
    switch (state) {
        case 0:
            if (work->field_3C6 == 0) {
                scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                delta->vz         = dz;
                dx                = scratchEnd[-1].vx;
                if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                    value = 1;
                }
            }
            if ((value != 0) || ((s8)Gp_StateF0.field_22 != 0) || (Gp_StateF0.field_8 != 0)) {
                if (work->field_3C6 == 0) {
                    D_80115412 = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C        = 1;
                work->field_392        = 7;
                work->field_3A8        = Actor02600_D08998[((GpEnemy*)actor->spawnArg2)->place->rowIndex];
                work->field_2E4.coord  = coord;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(&Actor02600_D08950, 5);
                work->field_2E4.flags |= 0x8000;
                if ((sessionFlags & 0xFFFF0000) == 0x05200000) {
                    value = ((((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x55200006;
                    pan0  = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(value, (s32)pan0, (s8)gpGetObjDepth(coord));
                }
            } else if (work->field_3D0 != 0) {
                if (work->field_3C6 == 0) {
                    Gp_StateF0.field_22 = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C        = 2;
                work->field_392        = 9;
                work->field_3A8        = Actor02600_D08998[((GpEnemy*)actor->spawnArg2)->place->rowIndex];
                work->field_3BC        = 0x2D;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.coord  = coord;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(&Actor02600_D08950, 5);
                work->field_2E4.flags |= 0x8000;
                if ((sessionFlags & 0xFFFF0000) == 0x05200000) {
                    value = ((((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x55200006;
                    pan1  = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(value, (s32)pan1, (s8)gpGetObjDepth(coord));
                }
            }
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            work->field_370 = coord->workm;
            break;
        case 1:
            work->field_3A0 = (u16)work->field_3A0 + ((u16)work->field_35C.vy - (u16)coord->coord.t[1]);
            if (((D_80073B8C[0]->t[1] - 0x3E8) < coord->coord.t[1]) || (work->field_3D0 != 0) || (work->field_3CE != 0)) {
                work->field_39C = 2;
                work->field_392 = 9;
                work->field_3BC = 0x2D;
            }
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            work->field_370 = coord->workm;
            break;
        case 2:
            work->field_3A8 = ((s16)work->field_396 >= 0xC) << 7;
            if (work->field_3CC != 0) {
                work->field_39C        = 3;
                work->field_392        = 0xA;
                work->field_3A8        = 0x80;
                work->field_2E4.flags &= 0x7FFF;
                value                  = ((((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x401A0002;
                pan2                   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(value, (s32)pan2, (s8)gpGetObjDepth(coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 >= 0x1E) {
                Gp_ArmStateF0(1);
                work->field_39A        = state;
                work->field_39C        = 0;
                work->field_392        = 1;
                index                  = ((GpEnemy*)actor->spawnArg2)->place->rowIndex;
                work->field_39E        = Actor02600_D08978[index] + (((random = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
                work->field_2E4.coord  = ((TmdObject*)actor->extra)->coords + 4;
                work->field_2E4.radius = 0xC8;
                work->field_2E4.pos.vy = 0;
                work->field_3C8        = 0;
                Gp_LcgState            = random;
                if (((GpEnemy*)actor->spawnArg2)->hp <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    actor->state    = 2;
                }
            }
            break;
    }
    Actor02600_Fn02C94(actor);
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void Actor02600_Fn00FA0(Task* arg0)
{
    Actor202600Work*       work;
    GsCOORDINATE2*         coord;
    s32                    state;
    s16                    timer;
    s16                    timer2;
    s32                    distance;
    s32                    sound;
    s32                    dx;
    s32                    dz;
    s32                    pan;
    u32                    random;
    u32                    random2;
    Actor202600RotScratch* delta;
    Actor202600RotScratch* scratchEnd;

    scratchEnd                                        = *(Actor202600RotScratch**)PSX_SCRATCH_ADDR(0x3FC);
    delta                                             = scratchEnd - 1;
    *(Actor202600RotScratch**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                                              = arg0->work;
    state                                             = work->field_39C;
    coord                                             = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            work->field_3C8 = 0;
            work->field_398 = 0;
            work->field_3A6 = 0;
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39C = 1;
                work->field_392 = 2;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor02600_D08988[((GpEnemy*)arg0->spawnArg2)->place->rowIndex] + ((random >> 0x10) & 0x3FF);
                Gp_LcgState     = random;
                return;
            }
            return;
        case 1:
            scratchEnd[-1].vec.vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            delta->vec.vy         = 0;
            delta->vec.vz         = (s32)(Player_Status.coordMtx->t[2] - coord->coord.t[2]);
            work->field_3A4       = ratan2((s32)(s16)scratchEnd[-1].vec.vx, (s32)(s16)delta->vec.vz) & 0xFFF;
            work->field_3A6       = 0x12;
            if ((s16)work->field_396 >= 0xB) {
                work->field_398 = 0x17;
            }
            timer2          = (u16)work->field_39E - (u16)work->field_398;
            work->field_39E = timer2;
            if (timer2 <= 0) {
                work->field_39C = 0;
                work->field_392 = state;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor02600_D08978[((GpEnemy*)arg0->spawnArg2)->place->rowIndex] + ((random2 >> 0x10) & 0xF);
                Gp_LcgState     = random2;
                return;
            }
            if ((s16)work->field_396 == 0xC) {
                sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401A0001;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_396 >= 0x29) {
                work->field_396 = 0xB;
            }
            if (work->field_3A4 == work->field_3A2) {
                scratchEnd[-1].vec.vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
                delta->vec.vy         = 0;
                dz                    = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                delta->vec.vz         = dz;
                dx                    = scratchEnd[-1].vec.vx;
                distance              = SquareRoot0((dx * dx) + (dz * dz));
                if ((work->field_3C0 == 0) && (distance < 0x578) && (work->field_3B0 == 0) && !(Player_Status.peStateFlags & 1)) {
                    work->field_39A = 4;
                    work->field_39C = 0;
                    work->field_392 = 3;
                    work->field_3AC = 0;
                } else if (distance < 0x9C4) {
                    work->field_39A = 5;
                    work->field_39C = 0;
                    work->field_392 = 4;
                }
            }
            break;
    }
}

void Actor02600_Fn012E8(Task* arg0)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s32              sound;
    s32              pan;
    u32              random;

    work            = arg0->work;
    coord           = ((TmdObject*)arg0->extra)->coords;
    work->field_398 = 0;
    work->field_3A6 = 0;
    if ((s16)work->field_396 == 0x28) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401A0003;
        pan   = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
    }
    if ((u32)(work->field_396 - 0x2B) < 7U) {
        Gp_SpawnEnemyFromTable(work->field_36C, 1, 0, arg0->spawnArg2);
        work->field_3AC = (u16)(work->field_3AC + 1);
    }
    if ((s16)work->field_396 >= (Actor02600_D08A16 + 0x3C)) {
        work->field_39A = 3;
        work->field_39C = 0;
        work->field_392 = 1;
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_39E = Actor02600_D08978[((GpEnemy*)arg0->spawnArg2)->place->rowIndex] + ((random >> 0x10) & 0xF);
        Gp_LcgState     = random;
    }
}

void Actor02600_Fn0143C(Task* arg0)
{
    Actor202600Work*  work;
    GsCOORDINATE2*    coord;
    SVECTOR*          scratchEnd;
    register SVECTOR* allocated asm("v1");
    SVECTOR*          rotation;
    s16(*motion0)[2];
    s16(*motion1)[2];
    s16 state;
    s32 sound;
    s32 index;
    s32 pan;
    s32 pan1;
    u32 random;

    scratchEnd                          = *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    allocated                           = scratchEnd - 1;
    rotation                            = allocated;
    *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) = allocated;
    work                                = arg0->work;
    state                               = work->field_39C;
    coord                               = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            work->field_3C8 = 0;
            work->field_3CA = 0;
            work->field_398 = 0;
            work->field_3A6 = 0;
            if ((u32)(work->field_396 - 0x1B) < 0xDU) {
                work->field_3C8        = 1;
                work->field_3CA        = 1;
                work->field_2E4.flags |= 0xC000;
                if (work->field_3B0 == 0) {
                    index = (s16)work->field_396 < 0x22;
                } else {
                    index = 3;
                    if ((s16)work->field_396 < 0x22) {
                        index = 4;
                    }
                }
                work->field_2E4.key = Gp_PackPair(&Actor02600_D08950, index);
                if (((s16)work->field_396 < 0x23) && ((work->field_3D0 != 0) || (work->field_3CE != 0))) {
                    work->field_39C        = 1;
                    work->field_3CA        = 0;
                    work->field_392        = 5;
                    work->field_2E4.flags &= 0x3FFF;
                    break;
                }
            } else {
                work->field_2E4.flags &= 0x3FFF;
            }
            index   = 0;
            motion0 = Actor02600_D08A30;
            for (; index < 9; index++, motion0++) {
                if ((s16)work->field_396 <= (motion0[0][0] + Actor02600_D08A18)) {
                    coord->coord.t[0] += (s32)(motion0[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion0[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((s16)work->field_396 == 0x28) {
                sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401A0002;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
                work->field_3C8 = 0;
                if (((GpEnemy*)arg0->spawnArg2)->hp <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                }
            }
            if ((s16)work->field_396 >= (Actor02600_D08A18 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor02600_D08978[((GpEnemy*)arg0->spawnArg2)->place->rowIndex] + ((random >> 0x10) & 0xF);
                Gp_LcgState     = random;
            }
            break;
        case 1:
            index           = 0;
            motion1         = Actor02600_D08A54;
            work->field_398 = 0;
            work->field_3A6 = 0;
            for (; index < 9; index++, motion1++) {
                if ((s16)work->field_396 <= motion1[0][0]) {
                    coord->coord.t[0] += (s32)(motion1[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion1[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((u32)(work->field_396 - 0x1F) < 0xFU) {
                coord->coord.t[0] += (s32)(rcos((s32)work->field_3A2) * 0xB) >> 0xC;
                coord->coord.t[2] += (s32)(rsin((s32)work->field_3A2) * 0xB) >> 0xC;
            }
            if ((s16)work->field_396 == 0x10) {
                sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401A0002;
                pan1  = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan1, (s8)gpGetObjDepth(coord));
                work->field_3C8 = 2;
                if (((GpEnemy*)arg0->spawnArg2)->hp <= 0) {
                    work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx    = 0;
                    rotation->vy    = (u16)work->field_3A2 + 0x800;
                    rotation->vz    = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                }
            }
            if ((s16)work->field_396 >= 0x46) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                work->field_39E = Actor02600_D08978[((GpEnemy*)arg0->spawnArg2)->place->rowIndex] + (((Gp_LcgState = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
                work->field_3AA = 1;
                if (work->field_3C8 == 2) {
                    work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx    = 0;
                    rotation->vy    = (u16)work->field_3A2 + 0x800;
                    rotation->vz    = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->field_3C8 = 0;
                }
                for (index = 1; index < 8; index++) {
                    func_800B4114(work, index, (s32)work->field_392, 0, 0);
                }
            }
            break;
    }
    *(s32*)PSX_SCRATCH_ADDR(0x3FC) += 8;
}

/// Behaviour state 6, entered when a hit does damage. On entry it starts
/// animation 0xB, stops the forward and turn steps and plays sound
/// 0x401A0004 with the top nibble of the context's `field_8` in bits 8-11,
/// panned to the actor. Once the
/// animation has run 0x15 frames it goes to state 7 when `field_3D2` is 1,
/// otherwise to state 3 with animation 1 and a random 0..15 in `field_39E`.
void Actor02600_Fn01A0C(Task* arg0)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              sound;
    s32              pan;
    u32              random;

    work  = arg0->work;
    state = work->field_39C;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            work->field_392 = 0xB;
            work->field_394 = 1;
            work->field_39C = 1;
            work->field_398 = 0;
            work->field_3A6 = 0;
            sound           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401A0004;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            return;
        case 1:
            if ((s16)work->field_396 >= 0x15) {
                if (work->field_3D2 == state) {
                    work->field_39A = 7;
                    work->field_39C = 0;
                    return;
                }
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = state;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = (random >> 0x10) & 0xF;
            } else {
                return;
            }
            break;
    }
}

void Actor02600_Fn01B30(Task* arg0)
{
    TmdObject*       obj;
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s16(*motion)[2];
    SVECTOR* scratchEnd;
    SVECTOR* velocity;
    s32      state;
    s32      one;
    s32      pan1;
    s32      pan2;
    s32      pan3;
    s16      timer;
    GpEnemy* ctx;
    s32      indexOrSound;
    u32      randomY;
    u32      randomZ;
    u32      randomX;
    u32      randomDelay;
    u32      randomRise;

    obj        = arg0->extra;
    work       = arg0->work;
    ctx        = arg0->spawnArg2;
    scratchEnd = *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    velocity   = (*(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) = scratchEnd - 1);
    state      = work->field_39C;
    coord      = obj->coords;
    one        = 1;
    switch (state) {
        case 0:
            work->field_294.flags &= 0x7FFF;
            work->field_214.flags &= 0xBFFF;
            obj->flags             = (u16)obj->flags | 0x84;
            ctx->node.flags        = one;
            if (D_8011540E == one) {
                if (work->field_3C2 == 0) {
                    work->field_39E = Actor02600_D089A8[work->field_3C4];
                } else {
                    work->field_39E = Actor02600_D089D8[work->field_3C4];
                }
                work->field_39C = 1;
            }
            break;
        case 1:
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39E = 0;
                work->field_39C = 2;
            }
            break;
        case 2:
            Tmd_AllocBuffers((TmdObject*)obj);
            obj->flags   = (u16)obj->flags & 0xFFFB;
            indexOrSound = 0;
            if (work->field_3C2 == 0) {
                work->field_39C = 3;
                work->field_392 = 4;
                work->field_398 = 0;
                work->field_3A6 = 0;
                work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
            } else {
                work->field_392        = 7;
                work->field_39A        = state;
                work->field_39C        = 1;
                work->field_3A8        = Actor02600_D089E0[work->field_3C4];
                work->field_294.flags |= 0x8000;
                work->field_214.flags |= 0x4000;
                do {
                    velocity->vx = 0;
                    velocity->vz = 0;
                    randomRise   = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomRise;
                    velocity->vy = ((randomRise >> 0x10) & 0x1FF) + 0x2EE;
                    Gp_SpawnEff(0x6017C, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 5);
                indexOrSound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x510D0012;
                pan1         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan1, (s8)gpGetObjDepth(coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 == 0x1E) {
                indexOrSound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x51090007;
                pan2         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan2, (s8)gpGetObjDepth(coord));
            }
            indexOrSound = 0;
            if ((s16)work->field_396 == 0x27) {
                do {
                    randomX      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomX;
                    velocity->vx = -((coord->coord.m[0][2] * (s32)(((randomX >> 16) & 0x3F) + 0xAF)) >> 12);
                    randomY      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomY;
                    velocity->vy = ((randomY >> 16) & 0x1FF) - 0x6D6;
                    randomZ      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomZ;
                    velocity->vz = -((coord->coord.m[2][2] * (s32)(((randomZ >> 16) & 0x3F) + 0xAF)) >> 12);
                    Gp_SpawnEff(0x60051, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 3);
                indexOrSound = 0;
            }
            motion = Actor02600_D08A30;
            do {
                indexOrSound++;
                if ((s16)work->field_396 <= ((*motion)[0] + Actor02600_D08A18)) {
                    coord->coord.t[0] += ((*motion)[1] * rsin(work->field_3A2)) >> 12;
                    coord->coord.t[2] += ((*motion)[1] * rcos(work->field_3A2)) >> 12;
                    break;
                }
                motion++;
            } while (indexOrSound < 9);
            if ((s16)work->field_396 == 0x28) {
                indexOrSound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401A0002;
                pan3         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan3, (s8)gpGetObjDepth(coord));
                work->field_3A8        = 0x80;
                work->field_294.flags |= 0x8000;
                work->field_214.flags |= 0x4000;
            }
            if ((s16)work->field_396 >= (Actor02600_D08A18 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                randomDelay     = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor02600_D08978[((GpEnemy*)arg0->spawnArg2)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                Gp_LcgState     = randomDelay;
                Gp_ArmStateF0(1);
            }
            break;
    }
    *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Status-effect step, run every frame while `field_3B0` is set. `field_3B2`
/// cycles through 0x50 frames; every 12 frames effect 3 is spawned, alternating
/// between model nodes 3 and 5; every 0x24 frames sound 0x401A0005 is played
/// with the top nibble of the context's `field_8` in bits 8-11, panned to the
/// actor.
void Actor02600_Fn020D4(Task* arg0)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s32              sound;
    s32              pan;
    u16              timer;
    u16              effectTimer;
    u16              countdown;

    work            = arg0->work;
    coord           = ((TmdObject*)arg0->extra)->coords;
    timer           = work->field_3B2 + 1;
    work->field_3B2 = timer;
    if ((s16)timer >= 0x50) {
        work->field_3B2 = 0U;
    }
    effectTimer     = work->field_3B4 + 1;
    work->field_3B4 = effectTimer;
    if ((s16)effectTimer == 0xC) {
        work->field_3B4 = 0U;
        if (work->field_3B6 == 0) {
            func_800FDB18(3, ((TmdObject*)arg0->extra)->coords + 3, NULL, &work->field_354);
            work->field_3B6 = 1;
        } else {
            func_800FDB18(3, ((TmdObject*)arg0->extra)->coords + 5, NULL, &work->field_354);
            work->field_3B6 = 0;
        }
    }
    countdown       = work->field_3BE - 1;
    work->field_3BE = countdown;
    if ((s16)countdown <= 0) {
        work->field_3BE = 0x24U;
        sound           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401A0005;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(coord));
    }
}

/// Turn step, run every frame while `field_3A6` is non-zero: reads the
/// heading back from the coordinate, turns it toward `field_3A4` by at most
/// `field_3A6` the shorter way round the circle, keeps the result in
/// `field_3A2` and rebuilds the coordinate's rotation as that pure yaw.
void Actor02600_Fn02214(Task* arg0)
{
    Actor202600Work*       work;
    GsCOORDINATE2*         coord;
    Actor202600RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor202600RotScratch*)(SCRATCH_SP -= 0x18);
    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_3A4;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_3A2 = ang;
    if (adiff < 0x800) {
        step = work->field_3A6;
        if (step >= adiff) {
            work->field_3A2 = want;
        } else {
            next = work->field_3A2;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_3A2 = next;
        }
    } else {
        step = work->field_3A6;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_3A2 = work->field_3A4;
        goto done;
    turn:
        wrapStep = work->field_3A6;
        cur      = work->field_3A2;
        if (diff > 0) {
            work->field_3A2 = cur - wrapStep;
        } else {
            work->field_3A2 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_3A2;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

#undef SCRATCH_SP

/// Dying sequence, the actor task's third state. While the global mode is 1
/// only the colour is updated, and mode 2 sets the model's `field_C` to 0x80.
/// Otherwise the work's `field_39C` steps: step 0 saves the coordinate in
/// `field_370` for the squash, unlinks the context node and the work's four
/// collision objects, passes 0x37 (0x1A when `field_3C0` is clear) to
/// `Gp_ReleaseStateF0Add` and starts animation 0xB; step 1 squashes the model
/// (and, once `field_3BA` has passed 1, frees its buffers and spawns the model
/// effect of `Actor02600_Fn03C4C` in its place), spawns effect 0x600A5 at frame
/// 0xF and moves to step 2 at frame 0x3C; step 2 destroys the enemy 0x3C
/// frames later.
void Actor02600_Fn02364(GpEnemy* arg0, Task* arg1)
{
    VECTOR           vec;
    Actor202600Work* work;
    Actor202600Work* initialWork;
    Actor202600Work* dyingWork;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   colorCoord;
    TmdObject*       obj;
    s16              initialAnim;
    s16              dyingAnim;
    s16              releasePhase;
    s16              state;
    s32              releaseId;
    s32              initialIndex;
    s32              dyingIndex;
    u16              age;
    u16              destroyAge;

    obj   = arg1->extra;
    work  = arg1->work;
    coord = obj->coords;
    switch ((s32)Gp_StateF0.field_4) {
        case 1:
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
            return;
        case 2:
            obj->flags = 0x80;
            return;
        case 0:
        default:
            state = work->field_39C;
            switch (state) {
                case 0:
                    work->field_3A0 = 0x1000;
                    work->field_370 = coord->coord;
                    arg0->recs      = 0;
                    Gp_UnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->field_214);
                    Gp_UnlinkObj(&work->field_294);
                    Gp_UnlinkObj(&work->field_2E4);
                    Gp_UnlinkObj(&work->field_31C);
                    releaseId = 0x37;
                    if (work->field_3C0 == 0) {
                        releaseId = 0x1A;
                    }
                    Gp_ReleaseStateF0Add(arg1, releaseId);
                    Gp_SetStateF0Byte3(2);
                    work->field_39E = 0U;
                    work->field_39C = 1;
                    Gp_SetLightMode(arg0, 1);
                    if (work->field_3BA != 0) {
                        obj->flags = 0x80;
                    }
                    work->field_392 = 0xB;
                    initialWork     = arg1->work;
                    initialIndex    = 1;
                    if (initialWork->field_392 != initialWork->field_394) {
                        initialWork->field_394 = (s16)(u16)initialWork->field_392;
                        initialWork->field_396 = 0U;
                        initialAnim            = Actor02600_D08A10[initialWork->field_392];
                        do {
                            func_800B4114(initialWork, initialIndex, (s32)initialWork->field_392, 0,
                                          (s32)initialAnim);
                            initialIndex += 1;
                        } while (initialIndex < 8);
                    } else {
                        TOUCH_REG(initialIndex);
                        initialWork->field_396 += initialIndex;
                        do {
                            Gp_AnimTickIndex((GpAnimCtx*)initialWork, initialIndex);
                            initialIndex += 1;
                        } while (initialIndex < 8);
                    }
                    colorCoord = ((TmdObject*)arg1->extra)->coords;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
                    return;
                case 1:
                    releasePhase = work->field_3BA;
                    if (releasePhase != 0) {
                        if (releasePhase >= 2) {
                            work->field_3BA = 0;
                            Tmd_FreeBuffers((TmdObject*)obj);
                            obj->flags |= 4;
                            Actor02600_Fn03C4C(arg1);
                            Actor02600_Fn03D38(arg1);
                        } else {
                            work->field_3BA = (s16)((u16)work->field_3BA + 1);
                        }
                    }
                    Actor02600_Fn03B58(arg1);
                    age             = work->field_39E + 1;
                    work->field_39E = age;
                    if ((s16)age == 0xA) {
                        obj->flags = 2;
                    }
                    if ((s16)work->field_39E == 0xF) {
                        Gp_SpawnEff(0x600A5, coord, 2, NULL);
                    }
                    if ((s16)work->field_39E >= 0x3C) {
                        work->field_39C = 2;
                        work->field_39E = 0U;
                        obj->flags      = 0x80;
                    }
                    dyingWork  = arg1->work;
                    dyingIndex = 1;
                    if (dyingWork->field_392 != dyingWork->field_394) {
                        dyingWork->field_394 = (s16)(u16)dyingWork->field_392;
                        dyingWork->field_396 = 0U;
                        dyingAnim            = Actor02600_D08A10[dyingWork->field_392];
                        do {
                            func_800B4114(dyingWork, dyingIndex, (s32)dyingWork->field_392, 0,
                                          (s32)dyingAnim);
                            dyingIndex += 1;
                        } while (dyingIndex < 8);
                    } else {
                        TOUCH_REG(dyingIndex);
                        dyingWork->field_396 += dyingIndex;
                        do {
                            Gp_AnimTickIndex((GpAnimCtx*)dyingWork, dyingIndex);
                            dyingIndex += 1;
                        } while (dyingIndex < 8);
                    }

                    colorCoord = ((TmdObject*)arg1->extra)->coords;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
                    return;

                case 2:
                    destroyAge      = work->field_39E + 1;
                    work->field_39E = destroyAge;
                    if ((s16)destroyAge >= 0x3C) {
                        Gp_DestroyEnemy((GpEnemy*)arg0, arg1);
                    }
                    return;
            }
            break;
    }
}

/// Per-frame tick of the homing projectile: while the global mode is 1 the
/// frame is just drawn, in mode 2 nothing happens at all, and otherwise the
/// work is stepped. A live collision record whose kind is not 0x10 drops the
/// object's 0x8000 linked bit and wipes the record, which sends the tick
/// straight past the frame counter. Every other frame the work's flags mirror
/// the low two bits of the counter, the coordinate is advanced along its own
/// forward axis by `field_3A`, and the counter is bumped; at 0xF frames the
/// object is unlinked and the actor switches to state 2, otherwise `field_3A`
/// decays by an LCG-derived 0..0x1F and clamps at zero.
void Actor02600_Fn02780(GpEnemy* arg0, Task* arg1)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s16              age;
    s16              speed;
    s32              contact;
    u16              flags;
    u32              random;

    coord = ((TmdObject*)arg1->extra)->coords;
    work  = arg1->work;
    switch ((s32)Gp_StateF0.field_4) {
        case 1:
            Actor02600_Fn02954(arg1, work->field_38);
            return;
        default:
        default_case:
            contact = work->rec.key;
            if (contact != 0) {
                if ((contact & 0xFFFF0000) != 0x100000) {
                    work->obj.flags &= 0x7FFF;
                    Gp_ClearRec18Occupied(&work->rec);
                    goto block_7;
                }
                goto block_11;
            }
        block_7:
            if (!((u16)work->field_38 & 3)) {
                flags = work->obj.flags | 0xC000;
            } else {
                flags = work->obj.flags & 0x3FFF;
            }
            work->obj.flags    = flags;
            coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_3A) >> 0xC;
            coord->coord.t[1] += (s32)(coord->coord.m[1][2] * work->field_3A) >> 0xC;
            coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_3A) >> 0xC;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            Actor02600_Fn02954(arg1, work->field_38);
            age            = (u16)work->field_38 + 1;
            work->field_38 = age;
            if (age >= 0xF) {
            block_11:
                Gp_UnlinkObj(&work->obj);
                arg1->state = 2;
                return;
            }
            random         = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState    = random;
            speed          = (u16)work->field_3A - ((random >> 0x10) & 0x1F);
            work->field_3A = speed;
            if (speed < 0) {
                work->field_3A = 0;
            }
            return;
        case 0:
            goto default_case;
        case 2:
            return;
    }
}

/// Projects one frame's sprite into the scratch quad and, when the depth
/// clears the near plane, emits the semi-transparent `POLY_FT4` for it. The
/// model whose texture is drawn is the *parent* task's (`Task::parent`), not
/// this actor's, so the atlas and tpage come from whoever spawned it.
void Actor02600_Fn02954(Task* actor, s32 frame)
{
    POLY_FT4*               poly;
    GsCOORDINATE2*          coord;
    s32                     depth;
    s32                     screen;
    s32                     y;
    s32                     radius;
    s32                     x;
    s32                     bottom;
    s32                     top;
    s32                     left;
    s32                     right;
    Actor202600QuadScratch* scratchEnd;
    Actor202600QuadScratch* s;
    TmdObject*              texture;
    Actor202600Uv*          uv;
    SVECTOR*                projection;

    scratchEnd                     = (Actor202600QuadScratch*)*(u8**)PSX_SCRATCH_ADDR(0x3FC);
    coord                          = ((TmdObject*)actor->extra)->coords;
    actor                          = actor->parent;
    texture                        = actor->extra;
    scratchEnd[-1].p[0].vx         = (u16)coord->workm.t[0];
    s                              = scratchEnd - 1;
    s->p[0].vy                     = (u16)coord->workm.t[1];
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) = (u8*)s;
    s->p[0].vz                     = (u16)coord->workm.t[2];
    projection                     = &s->p[0];
    SOFT_TOUCH_REG(projection);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(projection);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].screen);
    gte_stszotz(&scratchEnd[-1].depth);
    depth = s->depth;
    if (depth >= 0x14) {
        radius                 = (s32)(Actor02600_D08A98[frame] * 0x300) / depth;
        poly                   = gGpuPrimCursor;
        screen                 = s->screen;
        gGpuPrimCursor         = (u8*)poly + 0x28;
        x                      = screen & 0xFFFF;
        y                      = screen >> 0x10;
        left                   = x - radius;
        top                    = y - radius;
        right                  = x + radius;
        bottom                 = y + radius;
        scratchEnd[-1].p[0].vx = left;
        s->p[0].vy             = top;
        s->p[0].vz             = 0;
        s->p[1].vx             = right;
        s->p[1].vy             = top;
        s->p[1].vz             = 0;
        s->p[2].vx             = left;
        s->p[2].vy             = bottom;
        s->p[2].vz             = 0;
        s->p[3].vx             = right;
        s->p[3].vy             = bottom;
        s->p[3].vz             = 0;
        setPolyFT4(poly);
        setSemiTrans(poly, 1);
        setRGB0(poly, 0x80, 0x80, 0x80);
        setShadeTex(poly, 1);
        poly->tpage = (s16)(((s32)(((texture->tpage << 6) + 0x180) & 0x3FF) >> 6) | 0xB0);
        poly->clut  = (s16)(((s32)((u8)texture->clut << 0x18) >> 0x12) + 0x3D40);
        uv          = &Actor02600_D08A78[frame >> 1];
        poly->u0    = (u8)uv->u;
        poly->v0    = (u8)uv->v;
        poly->u1    = (s8)(uv->u + 0x1F);
        poly->v1    = (u8)uv->v;
        poly->u2    = (u8)uv->u;
        poly->v2    = (s8)(uv->v + 0x1F);
        poly->u3    = (s8)(uv->u + 0x1F);
        poly->v3    = (s8)(uv->v + 0x1F);
        poly->x0    = (u16)scratchEnd[-1].p[0].vx;
        poly->y0    = (u16)s->p[0].vy;
        poly->x1    = (u16)s->p[1].vx;
        poly->y1    = (u16)s->p[1].vy;
        poly->x2    = (u16)s->p[2].vx;
        poly->y2    = (u16)s->p[2].vy;
        poly->x3    = (u16)s->p[3].vx;
        poly->y3    = (u16)s->p[3].vy;
        addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
    }
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) += 0x28;
}

/// Draws a vertical semi-transparent gouraud line in the space of the matrix
/// `field_370`, from height `field_3A0 - 0x352` to height -0x352; nothing is
/// drawn when either end projects nearer than depth 30. The line runs from grey
/// 0x80 to 0xC0; while `field_3BC` counts down (never below 1) both ends are
/// scaled by `field_3BC / 45`.
void Actor02600_Fn02C94(Task* actor)
{
    Actor202600LineScratch* s;
    Actor202600Work*        work;
    LINE_G2*                line;
    DR_TPAGE*               page;
    s32                     x;
    s32                     y;
    s32                     screen;
    s32                     screen1;
    s32                     shade;
    u8                      blue;

    s              = (Actor202600LineScratch*)(*(u8**)PSX_SCRATCH_ADDR(0x3FC) -= sizeof(Actor202600LineScratch));
    work           = actor->work;
    s->position.vx = 0;
    s->position.vy = work->field_3A0 - 0x352;
    s->position.vz = 0;
    gte_SetRotMatrix(&work->field_370);
    gte_SetTransMatrix(&work->field_370);
    gte_ldv0(&s->position);
    gte_rtps();
    gte_stsxy(&s->screen);
    gte_stszotz(&s->depth);
    if (s->depth < 30) {
        *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor202600LineScratch);
        return;
    }
    screen         = s->screen;
    x              = screen & 0xFFFF;
    y              = screen >> 16;
    s->position.vx = 0;
    s->position.vy = -0x352;
    s->position.vz = 0;
    gte_SetRotMatrix(&work->field_370);
    gte_SetTransMatrix(&work->field_370);
    gte_ldv0(&s->position);
    gte_rtps();
    gte_stsxy(&s->screen);
    gte_stszotz(&s->depth);
    if (s->depth < 30) {
        *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor202600LineScratch);
        return;
    }
    line           = gGpuPrimCursor;
    gGpuPrimCursor = (u8*)line + sizeof(LINE_G2);
    screen1        = s->screen;
    setLineG2(line);
    setSemiTrans(line, 1);
    line->x0 = x;
    line->y0 = y;
    line->x1 = screen1;
    line->y1 = screen1 >> 16;
    if (work->field_3BC == 0) {
        line->b0 = line->g0 = line->r0 = 0x80;
        line->b1 = line->g1 = line->r1 = 0xC0;
    } else {
        if (--work->field_3BC <= 0) {
            work->field_3BC = 1;
        }
        shade    = (work->field_3BC * 0x80) / 45;
        line->r0 = shade;
        TOUCH_REG(shade);
        blue = shade;
        TOUCH_REG_USE(blue, shade);
        line->g0 = shade;
        line->b0 = blue;
        shade    = (work->field_3BC * 0xC0) / 45;
        line->r1 = shade;
        TOUCH_REG(shade);
        blue = shade;
        TOUCH_REG_USE(blue, shade);
        line->g1 = shade;
        line->b1 = blue;
    }
    addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), line);
    page           = gGpuPrimCursor;
    gGpuPrimCursor = (u8*)page + sizeof(DR_TPAGE);
    setlen(page, 1);
    page->code[0] = 0xE1000620;
    addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), page);
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor202600LineScratch);
}

/// Spawn handler: allocates the actor's work block, links the four `GpObj`
/// nodes (two collision-record tables, the coordinates and the effect arg) and
/// seeds the initial state from the spawn parameters. The spawn mode splits
/// into a tens digit (unused) and a units digit: 0 and 1 either place the actor
/// on a table row and rotate it to face that row's angle, or fall through to
/// the "walk to the player" state, 2 and 3 lift the coordinate and arm a
/// timer. Modes >= 10 re-read the variant index from the parameters.
void Actor02600_Fn02FFC(GpEnemy* ctx, Task* actor)
{
    SVECTOR          rot;
    GpRec18*         rec0;
    GpRec18*         rec1;
    GpRec18*         rec2;
    GpRec18*         rec3;
    SVECTOR*         positions;
    MATRIX*          matrix;
    Actor202600Work* work;
    s32              variant;
    s32              quotient;
    s32              i;
    s32              mode;
    GpAreaPlace*     params;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;

    obj   = actor->extra;
    coord = obj->coords;
    work  = memCalloc(sizeof(Actor202600Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work     = (void*)work;
    obj->flags      = 0;
    coord->flg      = 0;
    obj->lightMtx   = &work->field_1F4;
    obj->colorMtx   = &work->field_1D4;
    matrix          = &coord->coord;
    work->field_3C0 = 0;
    work->field_36C = &Actor02600_D08AB4;
    ctx->field_4    = matrix;
    ctx->field_48   = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                 = ((TmdObject*)actor->extra)->coords + 1;
    ctx->bodyPos.vy            = -0x64;
    ctx->recs                  = work->field_2B4;
    ctx->bodyPos.vx            = 0;
    ctx->bodyPos.vz            = 0;
    ctx->param                 = &Actor02600_D08968;
    ctx->hp                    = (s16)Actor02600_D08968.hpMax;
    work->field_354.coord      = coord;
    work->field_354.spawnArgLo = 0x100;
    work->field_354.spawnArgHi = 1;
    work->field_3C4            = (s16)ctx->place->variant;
    params                     = ctx->place;
    mode                       = params->mode;
    if (mode < 10) {
        switch (mode) {
            case 0:
                work->field_392 = 0xC;
                work->field_39A = 0;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 1:
                work->field_39A = mode;
                work->field_392 = mode;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 2:
                work->field_39A    = mode;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 0;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
            case 3:
                work->field_39A    = 2;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 1;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
        }
    } else {
        work->field_3C4 = (s16)params->variant;
        quotient        = mode / 10;
        variant         = mode - quotient * 10;
        switch (variant) {
            case 0:
                if (GameFlag_GetNibble(0xCC) == 1) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = Actor02600_D089D0[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor02600_D089B0;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                } else {
                    work->field_3C2 = 0;
                    work->field_39A = 8;
                    work->field_392 = 1;
                    work->field_3A8 = 0;
                }
                break;
            case 1:
                if (GameFlag_GetNibble(0xCB) == 2) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = Actor02600_D08A08[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor02600_D089E8;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                    break;
                }
                work->field_39A    = 8;
                work->field_3C2    = variant;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C8    = variant;
                coord->coord.t[1] += 0x3E8;
        }
    }
    func_800B3F84((GpAnimCtx*)work, Actor02600_D08ACC, (TmdObject*)obj, work->field_154, ((Actor202600Anim*)work)->slots);
    for (i = 1; i < 8; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    rec0                     = work->field_234;
    work->field_214.coord    = coord;
    work->field_214.ctx.recs = rec0;
    work->field_214.pos.vx   = 0;
    work->field_214.pos.vy   = -0x12C;
    work->field_214.pos.vz   = 0;
    work->field_214.key      = 0x3001A;
    work->field_214.radius   = 0x12C;
    work->field_214.flags    = 1;
    Gp_LinkObj(2, &work->field_214);
    Gp_InitRec18Table(rec0, 4, 0);
    work->field_214.flags   |= 0x4200;
    work->field_294.coord    = ((TmdObject*)actor->extra)->coords + 1;
    rec1                     = work->field_2B4;
    work->field_294.ctx.recs = rec1;
    work->field_294.pos.vx   = 0;
    work->field_294.pos.vy   = -0x64;
    work->field_294.pos.vz   = 0;
    work->field_294.key      = 0x3001A;
    work->field_294.radius   = 0x12C;
    work->field_294.flags    = 1;
    Gp_LinkObj(2, &work->field_294);
    Gp_InitRec18Table(rec1, 2, 0);
    work->field_294.flags   |= 0x8000;
    work->field_2E4.coord    = ((TmdObject*)actor->extra)->coords + 4;
    rec2                     = work->field_304;
    work->field_2E4.ctx.recs = rec2;
    work->field_2E4.pos.vx   = 0;
    work->field_2E4.pos.vy   = 0;
    work->field_2E4.pos.vz   = 0;
    work->field_2E4.key      = 0;
    work->field_2E4.radius   = 0xC8;
    work->field_2E4.flags    = 1;
    Gp_LinkObj(3, &work->field_2E4);
    Gp_InitRec18Table(rec2, 1, 0);
    work->field_2E4.flags   &= 0x7FFF;
    work->field_31C.coord    = ((TmdObject*)actor->extra)->coords + 4;
    rec3                     = work->field_33C;
    work->field_31C.ctx.recs = rec3;
    work->field_31C.pos.vx   = 0;
    work->field_31C.pos.vy   = 0;
    work->field_31C.pos.vz   = 0;
    work->field_31C.key      = 0x22424;
    work->field_31C.radius   = 0x1F4;
    work->field_31C.flags    = 1;
    Gp_LinkObj(1, &work->field_31C);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_31C.flags &= 0x7FFF;
    actor->state           = 1;
}

/// Per-frame tick, selected by the global mode `Gp_StateF0.field_4`. Mode 1 only
/// updates the colour and the ground shadow; mode 2 sets the model's `field_C` to
/// 0x80 and the context's `field_14` to 1 and stops there; mode 0 clears both
/// and then runs the full tick like any other mode. The full tick applies the
/// timed status damage when the context flags ask for it, resolves the
/// collision records, runs the behaviour state, the effect step while
/// `field_3B0` is set and the turn step while `field_3A6` is, moves and
/// animates the actor and refreshes its coordinate.
void Actor02600_Fn03558(GpEnemy* arg0, Task* arg1)
{
    s32              state;
    TmdObject*       obj;
    Actor202600Work* work;
    GsCOORDINATE2*   coord;

    obj   = arg1->extra;
    state = Gp_StateF0.field_4;
    work  = arg1->work;
    coord = obj->coords;
    if (state == 1) {
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
    obj->flags       = 0;
    arg0->node.flags = 0;
    goto default_body;
case2:
    obj->flags       = 0x80;
    arg0->node.flags = 1;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor02600_Fn0366C(arg1);
    }
    Actor02600_Fn0006C(arg1);
    Actor02600_Fn03784(arg1);
    if (work->field_3B0 != 0) {
        Actor02600_Fn020D4(arg1);
    }
    if (work->field_3A6 != 0) {
        Actor02600_Fn02214(arg1);
    }
    Actor02600_Fn03910(arg1);
    Actor02600_Fn039A4(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor02600_Fn03A68(arg1);
    Actor02600_Fn03AC0(arg1);
}

/// Status handling, run when the context's `field_4C` flags are non-zero.
/// Flag 0x2 is consumed, unless `field_3C8` is 1, by switching to state 7 with
/// `field_3D2` set. While flag 0x4 or 0x8 is set, `Gp_TickObjFlag4` yields a
/// per-frame damage that is passed to `func_800DA6E8` and taken from the hit
/// points in `field_40`; outside `field_3C8` 1 the actor then enters state 9 when they
/// run out (setting `field_30` to 2) or state 6 otherwise. Both flags are
/// cleared once `Gp_ObjFlag4Expired` returns non-zero.
void Actor02600_Fn0366C(Task* arg0)
{
    Actor202600Work* work;
    s32              damage;
    s32              remaining;
    u8               flags;
    GpEnemy*         ctx;

    ctx   = arg0->spawnArg2;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if ((flags & 2) && (work->field_3C8 != 1)) {
        ctx->reactionFlags = (u8)(flags & 0xFD);
        work->field_39A    = 7;
        work->field_39C    = 0;
        work->field_3D2    = 1;
    }
    if (ctx->reactionFlags & 0xC) {
        damage = Gp_TickObjFlag4(ctx);
        if ((s16)damage != 0) {
            func_800DA6E8(&ctx->node, (s16)damage, 0);
            remaining = (u16)ctx->hp - damage;
            ctx->hp   = remaining;
            if (work->field_3C8 != 1) {
                if ((s16)remaining <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                } else {
                    work->field_39A = 6;
                    work->field_39C = 0;
                }
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags = (u8)(ctx->reactionFlags & 0xF3);
        }
    }
}

/// Runs the handler of the work's current behaviour state (`field_39A`, 0-8);
/// state 9, entered when the hit points run out, runs nothing.
void Actor02600_Fn03784(Task* arg0)
{
    switch (((Actor202600Work*)arg0->work)->field_39A) {
        case 0:
            Actor02600_Fn00754(arg0);
            break;
        case 1:
            Actor02600_Fn00914(arg0);
            break;
        case 2:
            Actor02600_Fn00A94(arg0);
            break;
        case 3:
            Actor02600_Fn00FA0(arg0);
            break;
        case 4:
            Actor02600_Fn012E8(arg0);
            break;
        case 5:
            Actor02600_Fn0143C(arg0);
            break;
        case 6:
            Actor02600_Fn01A0C(arg0);
            break;
        case 7:
            Actor02600_Fn0385C(arg0);
            break;
        case 8:
            Actor02600_Fn01B30(arg0);
            break;
        case 9:
            break;
    }
}

/// Behaviour state 7. On entry it starts animation 0xE and stops the forward
/// and turn steps; each frame after that `Gp_TickObjFlag2` is ticked on the
/// context, and when it returns non-zero the actor goes to state 3
/// with animation 0xB, `field_3D2` cleared and a random 0..15 in `field_39E`.
void Actor02600_Fn0385C(Task* arg0)
{
    Actor202600Work* work;
    s16              state;
    u32              random;

    work  = arg0->work;
    state = work->field_39C;
    switch (state) {
        case 0:
            work->field_392 = 0xE;
            work->field_398 = 0;
            work->field_3A6 = 0;
            work->field_39C = 1;
            return;
        case 1:
            if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 0xB;
                work->field_3D2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = (s16)((random >> 0x10) & 0xF);
            }
            return;
    }
}

/// Steps the actor's coordinate, saving the previous translation in
/// `field_35C` first. The horizontal step follows the coordinate's forward
/// axis (`coord.m[*][2]`) scaled by the speed `field_398`, where 0x1000 is one
/// unit; `field_3A8` is added to the height unscaled.
void Actor02600_Fn03910(Task* arg0)
{
    GsCOORDINATE2*   coord;
    Actor202600Work* work;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;

    work->field_35C.vx = coord->coord.t[0];
    work->field_35C.vy = coord->coord.t[1];
    work->field_35C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_398) >> 12;
    coord->coord.t[1] += work->field_3A8;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_398) >> 12;
}

/// Switches the work's animation id, resetting the slots to the blend value the
/// table gives for the new id; otherwise ticks every slot one frame.
void Actor02600_Fn039A4(Task* arg0)
{
    Actor202600Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    i    = 1;
    if (work->field_392 != work->field_394) {
        work->field_394 = work->field_392;
        work->field_396 = 0;
        value           = Actor02600_D08A10[work->field_392];
        for (; i < 8; i++) {
            func_800B4114(work, i, work->field_392, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_396 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 8);
    }
}

/// Passes the world position of the model's root coordinate to
/// `Gp_UpdateActorColor` for the context, with both trailing arguments 0.
void Actor02600_Fn03A68(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

/// Ground shadow under the actor's coordinate. With `field_39A` at 2 the
/// position is cast down to the ground by `func_800EA1A8` and the shade comes
/// from `func_800EA318`; otherwise the coordinate's own world translation is
/// used at full shade.
void Actor02600_Fn03AC0(Task* arg0)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_39A == 2) {
        hit = func_800EA1A8((VECTOR3*)coord->workm.t, &vec);
        if (hit != 0) {
            Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
        }
    } else {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x200, 0x80);
    }
}

/// Squashes the model vertically: `field_3A0` shrinks by 0x50 a frame while
/// above 0x200, and the root coordinate becomes the matrix `field_370` scaled
/// on Y by `field_3A0` (0x1000 = 1), built through a 0x30-byte scratchpad
/// block that is released again.
void Actor02600_Fn03B58(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor202600Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = ((TmdObject*)arg0->extra)->coords;
    if (work->field_3A0 >= 0x201) {
        work->field_3A0 = (u16)work->field_3A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_3A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_370;
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

void Actor02600_Fn03C4C(Task* actor)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    GpEffWork*   eff;
    TmdObject*   model;
    s32          idx;
    u32          raw;

    D_80067704[0] = Actor02600_D05F10;
    eff           = Gp_SpawnEff(0x40007, ((TmdObject*)actor->extra)->coords + 4, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
    raw        = ((GpEnemy*)actor->spawnArg2)->placeKey;
    model      = (TmdObject*)eff->task->extra;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// Shrinks the model's third coordinate node to 1/16 through a 0x30-byte
/// block taken from the scratchpad and released again: an identity rotation
/// is written word-wise, `ScaleMatrix` scales its diagonal to 0x100 and
/// `MulMatrix` multiplies it into `field_8[2].coord`.
void Actor02600_Fn03D38(Task* actor)
{
    void**                      scratch;
    void*                       head;
    ActorShared80135b58Scratch* blk;
    GsCOORDINATE2*              coord;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *scratch = blk;
    coord    = ((TmdObject*)actor->extra)->coords;

    blk->scale.vx          = 0x100;
    blk->scale.vy          = 0x100;
    blk->scale.vz          = 0x100;
    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;
    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord[2].coord, &blk->mat.mat);
    *scratch = (u8*)*scratch + 0x30;
}

void Actor02600_Fn03DD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02600_D0002C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Setup state of the projectile task: allocates its 0x40-byte work, places
/// its model's coordinate at the spawning model's fifth node (expressed
/// relative to the view coordinate), and links the work's collision object
/// with its single record, carrying the parent work's `field_3AC`. The enemy
/// is destroyed when the allocation fails.
void Actor02600_Fn03E2C(GpEnemy* enemy, Task* task)
{
    Task*                        parent;
    TmdObject*                   parentObj;
    GsCOORDINATE2*               coord;
    Actor202600Work*             parentWork;
    GsCOORDINATE2*               parentCoord;
    ActorsShared80135c4cObjWork* work;
    u16                          pair;

    parent      = task->parent;
    parentObj   = parent->extra;
    coord       = ((TmdObject*)task->extra)->coords;
    parentWork  = (Actor202600Work*)parent->work;
    parentCoord = &parentObj->coords[4];
    work        = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    coord->sub = &gGfxViewCoord;
    Gp_WorldToLocal(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->flg         = 0;
    work->field_3A     = 0xC0;
    pair               = parentWork->field_3AC;
    work->obj.coord    = coord;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = 0;
    work->obj.pos.vz   = 0;
    work->obj.ctx.recs = &work->rec;
    work->field_3C     = pair;
    work->obj.key      = Gp_PackPair(&Actor02600_D08950, 2);
    work->obj.radius   = 0x100;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(&work->rec, 1, 0);
    work->obj.flags |= 0xC000;
    task->state      = 1;
}

void Actor02600_Fn03F80(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02600_D00038;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
