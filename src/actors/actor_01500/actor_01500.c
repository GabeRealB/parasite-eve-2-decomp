#include "common.h"

#include "actors/actors_shared_80135b58.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The actor's animation work area. `field_352` is the pose the actor asks
/// for, `field_354` the pose its slots were last re-queued for and
/// `field_356` the frame count accumulated while the two agree:
/// `Actor01500_Fn02958` re-seeds the slots from the per-state id table
/// when they differ and ticks them while they match.
typedef struct Actor101500Work {
    /* 0x000 */ byte     pad_0[0x14];
    /* 0x014 */ byte     field_14[0x118]; // animation slots, handed to `func_800B3F84`
    /* 0x12C */ byte     field_12C[0x70]; // pose buffer, handed to `func_800B3F84`
    /* 0x19C */ MATRIX   field_19C;       // model colour matrix
    /* 0x1BC */ MATRIX   field_1BC;       // model light matrix
    /* 0x1DC */ GpObj    field_1DC;
    /* 0x1FC */ GpRec18  field_1FC[3];
    /* 0x244 */ GpObj    field_244;
    /* 0x264 */ GpRec18  field_264[5];
    /* 0x2DC */ GpObj    field_2DC;
    /* 0x2FC */ GpRec18  field_2FC[1];
    /* 0x314 */ GpEffArg field_314; // record the hit's effect is spawned with
    /* 0x31C */ VECTOR3  field_31C; // position before this frame's step
    /* 0x328 */ byte     pad_328[4];
    /* 0x32C */ MATRIX   field_32C;
    /* 0x34C */ s32      field_34C;
    /* 0x350 */ s16      field_350; // hit cooldown, reloaded from `Gp_GetIdParam2`
    /* 0x352 */ u16      field_352;
    /* 0x354 */ s16      field_354;
    /* 0x356 */ u16      field_356;
    /* 0x358 */ s16      field_358;
    /* 0x35A */ s16      field_35A;
    /* 0x35C */ s16      field_35C;
    /* 0x35E */ u16      field_35E;
    /* 0x360 */ s16      field_360;
    /* 0x362 */ s16      field_362;
    /* 0x364 */ s16      field_364;
    /* 0x366 */ s16      field_366;
    /* 0x368 */ s16      field_368;
    /* 0x36A */ s16      field_36A;
    /* 0x36C */ s16      field_36C;
    /* 0x36E */ s16      field_36E;
    /* 0x370 */ s16      field_370;
    /* 0x372 */ u16      field_372;
    /* 0x374 */ s16      field_374;
    /* 0x376 */ s16      field_376;
    /* 0x378 */ s16      field_378;
    /* 0x37A */ s16      field_37A;
    /* 0x37C */ s16      field_37C;
    /* 0x37E */ s16      field_37E;
    /* 0x380 */ s16      field_380;
    /* 0x382 */ s16      field_382; // spawn variant, `GpAreaPlace.variant`
} Actor101500Work;

/// 0x58-byte frame allocated on the scratchpad stack by
/// `Actor01500_Fn004EC`.
typedef struct Actor101500ContactFrame {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ VECTOR         normal;
    /* 0x40 */ VECTOR         push;
    /* 0x50 */ s16            dx;
    /* 0x52 */ byte           pad_52[2];
    /* 0x54 */ s16            dz;
    /* 0x56 */ byte           pad_56[2];
} Actor101500ContactFrame;
STATIC_ASSERT_SIZEOF(Actor101500ContactFrame, 0x58);

/// Per-state animation id handed to `func_800B4114`, indexed by `field_352`.
extern s16 Actor01500_D0A050[];

/// Fifteen vertical bob offsets cycled by `field_37C` while `field_352` is 5.
extern s16 Actor01500_D0A070[];

/// Sixteen frame counts the hovering states reload `field_362` from, picked
/// by a `Gp_LcgState` draw.
extern u16 Actor01500_D09FC8[];

/// Sixteen distances `field_35E` is reloaded from when the actor starts to
/// advance, picked by a `Gp_LcgState` draw.
extern u16 Actor01500_D09FE8[];

/// 0x18-byte frame allocated on the scratchpad stack; only the `SVECTOR` at
/// +0x10 is used, as the rotation `Actor01500_Fn01838` hands `RotMatrix`.
typedef struct Actor101500RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor101500RotScratch;
STATIC_ASSERT_SIZEOF(Actor101500RotScratch, 0x18);

extern u8      D_801153F2[2];
extern u8      D_801153F4;
extern MATRIX* D_80073B8C;

/* `D_80067704` selects the model stream the next `Gp_SpawnEff` builds its
 * `TmdObject` from. */
extern void* D_80067704[1];

void    func_800B4114(Actor101500Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);
MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Pair packed into the third collision object's `key` at spawn.
extern GpU16Pair Actor01500_D09FB4;
/// The enemy's parameter record; `hpMax` seeds the hit points.
extern GpPairSrcE Actor01500_D09FB8;
/// Animation bank handed to `func_800B3F84`.
extern u8 Actor01500_D0A014[];

/// The four model streams `Actor01500_Fn01AB0` spawns effects from.
extern u8 Actor01500_D0458C[];
extern u8 Actor01500_D046D0[];
extern u8 Actor01500_D048BC[];
extern u8 Actor01500_D04A94[];

/// Points `Actor01500_Fn020D8` measures against: the XZ of
/// `Actor01500_D0A090` is where it heads, its Y (`Actor01500_D0A092`) the
/// height it settles below, and `D_80073B8C` passing the X/Z bounds of
/// `Actor01500_D0A098` ends the state.
extern SVECTOR Actor01500_D0A090;
extern s16     Actor01500_D0A092;
extern SVECTOR Actor01500_D0A098;

void Actor01500_Fn00094(GpEnemy* arg0, Task* arg1);
void Actor01500_Fn004EC(Task* actor);
void Actor01500_Fn00AFC(Task* actor, s32 damage);
void Actor01500_Fn00CA4(Task* actor);
void Actor01500_Fn00FC4(Task* actor);
void Actor01500_Fn011B0(Task* actor);
void Actor01500_Fn015DC(Task* actor);
void Actor01500_Fn01708(Task* actor);
void Actor01500_Fn01838(Task* actor);
void Actor01500_Fn01988(Task* actor);
void Actor01500_Fn01AB0(Task* arg0);
void Actor01500_Fn01DF0(GpEnemy* arg0, Task* arg1);
void Actor01500_Fn020D8(Task* actor);
void Actor01500_Fn02428(Task* task);
void Actor01500_Fn02484(GpEnemy* enemy, Task* actor);
void Actor01500_Fn025C8(Task* actor);
void Actor01500_Fn026D8(Task* actor);
void Actor01500_Fn027B0(Task* actor);
void Actor01500_Fn0288C(Task* actor);
void Actor01500_Fn028B0(Task* actor);
void Actor01500_Fn02958(Task* actor);
void Actor01500_Fn02A1C(Task* actor);
void Actor01500_Fn02B14(Task* actor);
void Actor01500_Fn02B70(Task* actor);
void Actor01500_Fn02C34(Task* actor);

/// The actor's three task states - spawn, per-frame tick and teardown - run
/// by `Actor01500_Fn02428`.
const GpEnemyTaskFuncTable3 Actor01500_D00004 = {
    {
        Actor01500_Fn00094,
        Actor01500_Fn02484,
        Actor01500_Fn01DF0,
    },
};

/// Spawn handler: allocates the work area, binds the model and collision
/// objects, and seeds the pose from the spawn variant in `GpAreaPlace.variant`.
void Actor01500_Fn00094(GpEnemy* arg0, Task* arg1)
{
    Actor101500Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    GpRec18*         records;
    u32              draw;
    s32              i;
    s32              r;

    obj   = arg1->extra;
    coord = obj->coords;
    work  = memCalloc(0x384U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_1BC;
    obj->colorMtx  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &((TmdObject*)arg1->extra)->coords[2];
    arg0->node.flags           = 0;
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &Actor01500_D09FB8;
    arg0->recs                 = work->field_1FC;
    arg0->hp                   = Actor01500_D09FB8.hpMax;
    work->field_314.coord      = coord;
    work->field_314.spawnArgLo = 0x300;
    work->field_314.spawnArgHi = 1;
    place                      = arg0->place;
    switch (work->field_382 = place->variant) {
        case 0:
            work->field_36E = arg0->place->mode & 1;
            work->field_370 = (arg0->place->mode >> 1) & 1;
            switch (work->field_36E) {
                case 0:
                    work->field_352 = 1;
                    work->field_354 = 1;
                    work->field_34C = 0;
                    break;
                case 1:
                    work->field_352 = 2;
                    work->field_354 = 2;
                    work->field_34C = 0;
                    break;
            }
            work->field_362 = 0;
            draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->field_364    = ((draw >> 16) & 0x1F) + 1;
            work->field_372    = (ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) + 0x800) & 0xFFF;
            break;
        case 1:
            work->field_370 = 1;
            work->field_352 = 5;
            work->field_354 = 5;
            work->field_35A = 9;
            work->field_35C = 0;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_362 = ((Gp_LcgState >> 16) & 0x3F) + 0x3C;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_364 = (Gp_LcgState >> 16) & 0x1FF;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            break;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    func_800B3F84((GpAnimCtx*)work, Actor01500_D0A014, obj, work->field_12C,
                  (GpAnimSlot*)work->field_14);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, (s16)work->field_352);
    }
    if (work->field_382 == 0) {
        draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        r                  = (draw >> 16) & 0x3F;
        for (i = 1; i < 7; i++) {
            func_800B4114(work, i, work->field_352, 0, r);
        }
    }
    work->field_1DC.coord    = &((TmdObject*)arg1->extra)->coords[2];
    work->field_1DC.ctx.recs = work->field_1FC;
    work->field_1DC.pos.vx   = 0;
    work->field_1DC.pos.vy   = 0;
    work->field_1DC.pos.vz   = 0;
    work->field_1DC.key      = 0x3000F;
    work->field_1DC.radius   = 0x12C;
    work->field_1DC.flags    = 1;
    Gp_LinkObj(2, &work->field_1DC);
    Gp_InitRec18Table(work->field_1FC, 3, 0);
    work->field_244.ctx.recs = work->field_264;
    work->field_244.coord    = coord;
    work->field_244.pos.vx   = 0;
    work->field_1DC.flags   |= 0x8000;
    if (work->field_36E == 0) {
        work->field_244.pos.vy = 0;
        work->field_244.pos.vz = -0x12C;
    } else {
        work->field_244.pos.vy = 0x12C;
        work->field_244.pos.vz = 0;
    }
    work->field_244.key    = 0x3000F;
    work->field_244.radius = 0x12C;
    work->field_244.flags  = 1;
    Gp_LinkObj(2, &work->field_244);
    Gp_InitRec18Table(work->field_264, 5, 0);
    records                  = work->field_2FC;
    work->field_2DC.coord    = coord;
    work->field_2DC.ctx.recs = records;
    work->field_2DC.pos.vx   = 0;
    work->field_2DC.pos.vy   = 0;
    work->field_2DC.pos.vz   = 0x190;
    work->field_244.flags   |= 0x4200;
    work->field_2DC.key      = Gp_PackPair(&Actor01500_D09FB4, 0);
    work->field_2DC.radius   = 0x12C;
    work->field_2DC.flags    = 1;
    Gp_LinkObj(3, &work->field_2DC);
    Gp_InitRec18Table(records, 1, 0);
    work->field_2DC.flags &= 0x7FFF;
    arg1->state            = 1;
}

/// Per-frame contact pass: applies the collision step, reacts to the three
/// contact records (damage from actors, push-out from walls) and clears them.
void Actor01500_Fn004EC(Task* actor)
{
    Actor101500Work*         work;
    Actor101500ContactFrame* frame;
    s32                      push;
    VECTOR*                  normal;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           sourceCoord;
    GpRec18*                 effectRec;
    s16                      cooldown;
    s32                      result;
    s32                      i;
    s32                      depth;
    s32                      boundedDepth;
    s32                      dx;
    s32                      dy;
    s32                      dz;
    s32                      wallDx;
    s32                      wallDy;
    s32                      wallDz;
    u32                      lastId;
    u32                      id;
    u32                      hitId;
    u32                      damage;

    push                                    = 0;
    lastId                                  = 0;
    work                                    = actor->work;
    *(Actor101500ContactFrame**)0x1F8003FC -= 1;
    frame                                   = *(Actor101500ContactFrame**)0x1F8003FC;
    coord                                   = ((TmdObject*)actor->extra)->coords;
    result                                  = func_800E0C10(work->field_264, &frame->delta, 5, NULL);
    if (result != 0) {
        if (work->field_370 == 0 && work->field_35A == 3 && frame->delta.vy.w == 0) {
            work->field_35A        = 6;
            work->field_358        = 0;
            work->field_244.pos.vy = 0;
            work->field_244.pos.vz = -300;
            Gp_LcgState            = Gp_LcgState * 5 + 0x71357911;
            work->field_362        = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
            for (i = 0; i < 5; i++) {
                if ((work->field_264[i].key & 0xFFFF0000) == 0x100000) {
                    frame->dx       = work->field_264[i].at10.normal.vx;
                    frame->dz       = work->field_264[i].at10.normal.vz;
                    work->field_372 = (ratan2(frame->dx, frame->dz) + 0x800) & 0xFFF;
                    break;
                }
            }
        }
        if (work->field_35A == 4 && frame->delta.vy.w < -0xDDA) {
            work->field_35A = 5;
            work->field_362 = 0;
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += frame->delta.vx.h.hi;
                coord->coord.t[1] += frame->delta.vy.h.hi;
                coord->coord.t[2] += frame->delta.vz.h.hi;
                break;
            case 2:
                if (work->field_35A != 4) {
                    coord->coord.t[0] = work->field_31C.vx;
                    coord->coord.t[1] = work->field_31C.vy;
                    coord->coord.t[2] = work->field_31C.vz;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_264);
    if (work->field_350 != 0) {
        cooldown        = (u16)work->field_350 - 1;
        work->field_350 = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_350 = 0;
        }
    }
    normal = &frame->normal;
    for (i = 0; i < 3; i++) {
        id = work->field_1FC[i].key;
        switch (id >> 0x10) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_350 == 0) {
                    sourceCoord       = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
                    dx                = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vx.w = dx;
                    dy                = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vy.w = dy;
                    dz                = sourceCoord->coord.t[2] - coord->coord.t[2];
                    frame->delta.vz.w = dz;
                    damage            = Gp_ComputeDamage(work->field_1FC[i].key, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
                    if (Gp_RollEnemyChance(actor->spawnArg2, work->field_1FC[i].key, 0) != 0) {
                        damage *= 4;
                        Gp_SpawnEff(0x6009C, ((TmdObject*)actor->extra)->coords, 0, NULL);
                    }
                    func_800DA6E8(&((GpEnemy*)actor->spawnArg2)->node, damage, 0);
                    func_800E2C78((GpObj40*)actor->spawnArg2, work->field_1FC[i].key, damage, 0);
                    Actor01500_Fn00AFC(actor, damage);
                    switch (Gp_GetIdParam0(work->field_1FC[i].key) & 0xFFFF) {
                        case 0:
                        case 5:
                        case 7:
                            break;
                        case 1:
                            Gp_SetObjFlag1((GpObj4C*)actor->spawnArg2);
                            break;
                        case 3:
                            Gp_SetObjFlag4((GpObj5C*)actor->spawnArg2, work->field_1FC[i].key, 0);
                            break;
                        case 4:
                        case 6:
                            if (((GpEnemy*)actor->spawnArg2)->hp <= 0) {
                                work->field_37E = 1;
                            }
                            break;
                        case 2:
                        case 8:
                        case 9:
                            Gp_SetObjFlag2((GpObj5D*)actor->spawnArg2, work->field_1FC[i].key, 0);
                            break;
                    }
                    hitId = work->field_1FC[i].key;
                    if (lastId != hitId) {
                        lastId = hitId;
                        func_800FDB18(Gp_GetIdParam1(hitId) & 0xFFFF, coord, NULL, &work->field_314);
                    }
                    damage = Gp_GetIdParam2(work->field_1FC[i].key);
                    if ((s32)damage > 0) {
                        work->field_350 = damage;
                    }
                }
                break;
            case 3:
                wallDx            = coord->workm.t[0] - work->field_1FC[i].point.vx;
                frame->delta.vx.w = wallDx;
                wallDy            = coord->workm.t[1] - work->field_1FC[i].point.vy;
                frame->delta.vy.w = wallDy;
                wallDz            = coord->workm.t[2] - work->field_1FC[i].point.vz;
                frame->delta.vz.w = wallDz;
                depth             = work->field_1FC[i].depth - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
                boundedDepth      = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal((VECTOR*)&frame->delta, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, &frame->push);
                }
                break;
        }
    }
    if (push > 0) {
        coord->coord.t[0] += (s32)(push * frame->push.vx) >> 0xC;
        coord->coord.t[2] += (s32)(push * frame->push.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1FC);
    effectRec = work->field_2FC;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_2DC.flags &= 0x7FFF;
        Gp_ClearRec18Occupied(effectRec);
        work->field_36A = 1;
    }
    *(Actor101500ContactFrame**)0x1F8003FC += 1;
}

void Actor01500_Fn00AFC(Task* actor, s32 damage)
{
    GpEnemy*         enemy;
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s32              id;

    enemy      = actor->spawnArg2;
    work       = actor->work;
    coord      = ((TmdObject*)actor->extra)->coords;
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        work->field_378 = 1;
    }
    id = ((((GpEnemy*)actor->spawnArg2)->placeKey >> 12) << 8) | 0x400F0004;
    SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    if (enemy->hp <= (Actor01500_D09FB8.hpMax * 60) / 100) {
        work->field_358 = 2;
        if (work->field_35A != 5) {
            work->field_35A        = 4;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
        }
    } else {
        work->field_35A = 7;
        switch (work->field_358) {
            case 0:
                if (work->field_36E == 0) {
                    work->field_352 = 11;
                    work->field_354 = 1;
                } else {
                    work->field_352 = 12;
                    work->field_354 = 2;
                }
                break;
            case 1:
                work->field_352 = 13;
                work->field_354 = 6;
                break;
            case 2:
                work->field_352 = 13;
                work->field_354 = 14;
                break;
        }
        work->field_34C = 0;
        work->field_356 = 0;
    }
    Gp_SetStateF0Byte3(2);
}

/// Idle hover: waits for the player to come within 2500 units (then switches
/// to pose 3, or 4 when `field_36E` is set) or for a disturbance - a random
/// timeout, a `D_801153F2` trigger or lost hit points - that sends it into
/// pose 7/8 with a fresh `Actor01500_D09FC8` countdown.
void Actor01500_Fn00CA4(Task* actor)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          frame;
    s32              flag;
    s16              pose;
    s16              pose2;
    s16              val;

    *(VECTOR**)0x1F8003FC -= 1;
    frame                  = *(VECTOR**)0x1F8003FC;
    work                   = actor->work;
    coord                  = ((TmdObject*)actor->extra)->coords;
    flag                   = 0;
    if (work->field_37A != 0) {
        work->field_35A = 2;
        work->field_352 = 7;
        work->field_356 = 0;
        pose2           = Actor01500_D09FC8[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
        work->field_358 = 1;
        work->field_364 = 0;
        work->field_34C = 0x400F0002;
        work->field_380 = 0xF;
        work->field_362 = pose2;
    }
    work->field_376 = 0;
    frame->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    frame->vy       = 0;
    frame->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    if (SquareRoot0(frame->vx * frame->vx + frame->vz * frame->vz) < 2500) {
        work->field_35A = 1;
        pose            = 3;
        if (work->field_36E != 0) {
            pose = 4;
        }
        work->field_352 = pose;
        work->field_34C = 0x400F0001;
        work->field_362 = 0;
        work->field_364 = 0;
        Gp_ArmStateF0(1);
    } else {
        if (D_801153F2[0] & 1) {
            if (work->field_362 == 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_362 = ((Gp_LcgState >> 16) & 0x1F) + 1;
            }
        }
        if (work->field_362 != 0) {
            work->field_362--;
            if (work->field_362 <= 0) {
                flag = 1;
            }
        }
        work->field_364--;
        if (work->field_364 == 0) {
            if (D_801153F2[1] != 0) {
                flag = 1;
            }
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_364 = ((Gp_LcgState >> 16) & 0x1F) + 1;
        }
        if (((GpEnemy*)actor->spawnArg2)->hp != Actor01500_D09FB8.hpMax) {
            flag = 1;
        }
        if (flag != 0) {
            work->field_35A = 2;
            pose2           = 7;
            if (work->field_36E != 0) {
                pose2 = 8;
            }
            __asm__("" : "+r"(pose2), "=r"(val));
            work->field_352 = pose2;
            val             = Actor01500_D09FC8[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            work->field_358 = 1;
            work->field_37A = 1;
            work->field_364 = 0;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            work->field_362 = val;
            Gp_ArmStateF0(1);
        }
    }
    *(VECTOR**)0x1F8003FC += 1;
}

/// Once the pose has run 30 frames, re-aims `field_374` along the coordinate's
/// facing and walks the actor 40 units back along it (state 1 also rises, faster
/// early on); from frame 59 it queues pose 5 with two `Gp_LcgState` draws.
void Actor01500_Fn00FC4(Task* actor)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s16              angle;
    u32              rnd;
    u32              rnd2;

    work  = actor->work;
    coord = ((TmdObject*)actor->extra)->coords;
    if ((s16)work->field_356 >= 0x1E) {
        work->field_374 = angle = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        switch (work->field_36E) {
            case 0:
                coord->coord.t[0] += -(rsin(angle) * 40) >> 12;
                coord->coord.t[2] += -(rcos(work->field_374) * 40) >> 12;
                break;
            case 1:
                coord->coord.t[0] += -(rsin(angle) * 40) >> 12;
                coord->coord.t[2] += -(rcos(work->field_374) * 40) >> 12;
                if ((s16)work->field_356 < 0x24) {
                    coord->coord.t[1] += 0x6E;
                } else if ((s16)work->field_356 < 0x2E) {
                    coord->coord.t[1] += 0x23;
                } else {
                    coord->coord.t[1] += 0xF;
                }
                break;
        }
        if ((s16)work->field_356 >= 0x3B) {
            rnd             = Gp_LcgState * 5 + 0x71357911;
            work->field_35A = 3;
            work->field_352 = 5;
            work->field_35C = 0;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            Gp_LcgState     = rnd;
            work->field_362 = ((rnd >> 16) & 0x3F) + 0x3C;
            rnd2            = rnd * 5 + 0x71357911;
            Gp_LcgState     = rnd2;
            work->field_364 = (rnd2 >> 16) & 0x1FF;
        }
    }
}

/// Approach state, stepped by `field_35C`: settle vertically against the
/// height `D_80073B8C` gives while turning to the player, then advance by a
/// random `Actor01500_D09FE8` distance; within 1000 units of the player it
/// switches to pose 10 and rises until its collision object reports contact
/// or it passes the height limit, then settles again.
void Actor01500_Fn011B0(Task* actor)
{
    VECTOR3*         vec;
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    u32              seed;
    s32              off;
    u16              val;
    u16              val2;
    u16*             tbl;
    u8*              head;
    s32              diff;
    s32              dist;
    s32              y;
    s32              off2;
    s32              diff2;
    s32              dist2;

    head              = *(u8**)0x1F8003FC;
    *(u8**)0x1F8003FC = head - 0x10;
    vec               = (VECTOR3*)(head - 0x10);
    work              = actor->work;
    coord             = ((TmdObject*)actor->extra)->coords;
    switch (work->field_35C) {
        case 0:
            off  = work->field_364 + 0x708;
            diff = D_80073B8C->t[1] - off - coord->coord.t[1];
            dist = abs(diff);
            if (dist < 30 || --work->field_362 <= 0) {
                work->field_35C = 1;
            } else {
                work->field_366 = diff > 0 ? 30 : -30;
            }
            vec->vx                = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy                = 0;
            vec->vz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_372        = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376        = 100;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
            break;
        case 1:
            work->field_366 = 0;
            work->field_360 = 0;
            work->field_36C = 0;
            if (--work->field_362 < 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                val             = Actor01500_D09FE8[(Gp_LcgState >> 16) & 0xF];
                work->field_352 = 6;
                work->field_35C = 2;
                work->field_35E = val;
            }
            vec->vx         = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy         = 0;
            vec->vz         = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_372 = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376 = 100;
            break;
        case 2:
            work->field_360  = 200;
            work->field_35E -= 200;
            if ((s16)work->field_35E < 0) {
                tbl             = Actor01500_D09FC8;
                work->field_352 = 5;
                seed            = Gp_LcgState * 5 + 0x71357911;
                val2            = tbl[(seed >> 16) & 0xF];
                Gp_LcgState     = seed;
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_35C = 1;
                work->field_362 = val2;
            }
            if (work->field_36C == 0) {
                vec->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                vec->vy = 0;
                vec->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                if (SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz) < 1000) {
                    work->field_352        = 10;
                    work->field_35C        = 3;
                    work->field_34C        = 0;
                    work->field_36C        = 1;
                    work->field_2DC.flags |= 0x8000;
                }
            }
            break;
        case 3:
            diff2           = D_80073B8C->t[1] - 0x640;
            work->field_360 = 100;
            work->field_366 = 180;
            if (diff2 < coord->coord.t[1] || work->field_36A != 0) {
                work->field_35C        = 4;
                work->field_36A        = 0;
                work->field_352        = 6;
                Gp_LcgState            = Gp_LcgState * 5 + 0x71357911;
                work->field_362        = ((Gp_LcgState >> 16) & 0xF) + 15;
                work->field_2DC.flags &= 0x7FFF;
            }
            break;
        case 4:
            off2  = coord->coord.t[1] + 0x708;
            diff2 = D_80073B8C->t[1] - off2;
            dist2 = abs(diff2);
            if (dist2 < 0x60 || --work->field_362 <= 0) {
                work->field_35C = 2;
            } else {
                work->field_366 = diff2 > 0 ? 0x60 : -0x60;
            }
            break;
    }
    *(u8**)0x1F8003FC += 0x10;
}

/// Faces the actor toward the player on the XZ plane and raises `field_378`
/// once the player leaves the vertical band (500 above, 1800 below) or
/// `field_362` counts past 1800 frames.
void Actor01500_Fn015DC(Task* actor)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          head;
    VECTOR*          blk;

    work                      = actor->work;
    coord                     = ((TmdObject*)actor->extra)->coords;
    work->field_352           = 0xE;
    work->field_360           = 5;
    work->field_376           = 5;
    work->field_34C           = 0;
    work->field_366           = 0x80;
    head                      = *(VECTOR**)G_SCRATCH_HEAD;
    blk                       = head - 1;
    head[-1].vx               = coord->coord.t[0] - Player_Status.coordMtx->t[0];
    blk->vy                   = 0;
    blk->vz                   = coord->coord.t[2] - Player_Status.coordMtx->t[2];
    *(VECTOR**)G_SCRATCH_HEAD = blk;
    work->field_372           = ratan2((s16)head[-1].vx, (s16)blk->vz) & 0xFFF;
    if (coord->coord.t[1] > Player_Status.coordMtx->t[1] + 500 ||
        coord->coord.t[1] < Player_Status.coordMtx->t[1] - 1800 ||
        ++work->field_362 > 1800) {
        work->field_378 = 1;
    }
    *(VECTOR**)G_SCRATCH_HEAD += 1;
}

/// Leaves the idle poses once `field_356` frames have run: state 0 switches to
/// pose 7 (8 when `field_36E` is set), state 1 to pose 5 with a random
/// `field_362` delay, state 2 to pose 14.
void Actor01500_Fn01708(Task* actor)
{
    Actor101500Work* work = actor->work;
    s16              pose;
    u32              rnd;
    u16              val;
    u16*             tbl;

    switch (work->field_358) {
        case 0:
            pose = 7;
            if ((s16)work->field_356 >= 20) {
                work->field_35A = 0;
                if (work->field_36E != 0) {
                    pose = 8;
                }
                work->field_34C = 0x400F0002;
                work->field_352 = pose;
                work->field_35C = 0;
                work->field_380 = 15;
            }
            break;
        case 1:
            if ((s16)work->field_356 >= 10) {
                tbl             = Actor01500_D09FC8;
                work->field_35A = 3;
                work->field_352 = 5;
                work->field_35C = 0;
                rnd             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rnd;
                val             = tbl[(rnd >> 16) & 0xF];
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_362 = val;
            }
            break;
        case 2:
            if ((s16)work->field_356 > 0) {
                work->field_35A = 5;
                work->field_362 = 0;
                work->field_352 = 14;
                work->field_34C = 0;
            }
            break;
    }
}

/// Turns the actor toward `field_372` by at most `field_376` per call, taking
/// the short way round the 0x1000 circle, then rebuilds its rotation matrix.
void Actor01500_Fn01838(Task* arg0)
{
    Actor101500Work*       work;
    GsCOORDINATE2*         coord;
    Actor101500RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor101500RotScratch*)(SCRATCH_SP -= 0x18);
    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_372;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_374 = ang;
    if (adiff < 0x800) {
        step = work->field_376;
        if (step >= adiff) {
            work->field_374 = want;
        } else {
            next = work->field_374;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_374 = next;
        }
    } else {
        step = work->field_376;
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
        work->field_374 = work->field_372;
        goto done;
    turn:
        wrapStep = work->field_376;
        cur      = work->field_374;
        if (diff > 0) {
            work->field_374 = cur - wrapStep;
        } else {
            work->field_374 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_374;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

void Actor01500_Fn01988(Task* arg0)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s16              bob;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    bob   = 0;
    if ((s16)work->field_352 == 5) {
        work->field_37C++;
        if (work->field_37C >= 15) {
            work->field_37C = 0;
        }
        bob = Actor01500_D0A070[work->field_37C];
    }
    work->field_31C.vx = coord->coord.t[0];
    work->field_31C.vy = coord->coord.t[1];
    work->field_31C.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_360) >> 12;
    coord->coord.t[1] += work->field_366 + bob;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_360) >> 12;
    if (coord->coord.t[1] - D_80073B8C->t[1] > 5000) {
        arg0->state     = 2;
        work->field_35A = 8;
        work->field_35C = 4;
        work->field_362 = 0;
    }
}

void Actor01500_Fn01AB0(Task* arg0)
{
    GpAreaKey    key;
    u8           areaByte0;
    u32          raw1, index1;
    GpEffWork*   effect1;
    TmdObject*   model1;
    GpAreaPlace* entry1;
    GpAreaKey*   sessionKey1;
    u32          raw2, index2;
    GpEffWork*   effect2;
    TmdObject*   model2;
    GpAreaPlace* entry2;
    GpAreaKey*   sessionKey2;
    u32          raw3, index3;
    GpEffWork*   effect3;
    TmdObject*   model3;
    GpAreaPlace* entry3;
    GpAreaKey*   sessionKey3;
    u32          raw4, index4;
    GpEffWork*   effect4;
    TmdObject*   model4;
    GpAreaPlace* entry4;
    GpAreaKey*   sessionKey4;

    D_80067704[0] = Actor01500_D0458C;
    effect1       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x100, NULL);
    if (effect1 != NULL) {
        sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
        raw1        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model1      = (TmdObject*)effect1->task->extra;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->at4.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->tpage = entry1->tpage;
        model1->clut  = entry1->clut;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }

    D_80067704[0] = Actor01500_D046D0;
    effect2       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x100, NULL);
    if (effect2 != NULL) {
        sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
        raw2        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model2      = (TmdObject*)effect2->task->extra;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->at4.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->tpage;
        model2->clut  = entry2->clut;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }

    D_80067704[0] = Actor01500_D048BC;
    effect3       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x100, NULL);
    if (effect3 != NULL) {
        sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
        raw3        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model3      = (TmdObject*)effect3->task->extra;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->at4.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3        = (GpAreaPlace*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->tpage = entry3->tpage;
        model3->clut  = entry3->clut;
        if (model3->buffer != NULL) {
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }

    D_80067704[0] = Actor01500_D04A94;
    effect4       = Gp_SpawnEff(0x40007, &((TmdObject*)arg0->extra)->coords[1], 0x100, NULL);
    if (effect4 != NULL) {
        sessionKey4 = (GpAreaKey*)&gGameSession->at4.loc;
        raw4        = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        model4      = (TmdObject*)effect4->task->extra;
        key.stage   = sessionKey4->stage;
        key.area    = sessionKey4->area;
        key.room    = sessionKey4->room;
        areaByte0   = gGameSession->at4.loc.view;
        index4      = raw4 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry4        = (GpAreaPlace*)((index4 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model4->tpage = entry4->tpage;
        model4->clut  = entry4->clut;
        if (model4->buffer != NULL) {
            tmdProcessStream(model4);
            tmdProcessStream(model4);
        }
    }
}

/// Per-frame handler for the death sequence. Scene mode 1 only refreshes the
/// actor colour and mode 2 hides the model. Otherwise `field_35C` steps: state 0
/// saves the model matrix and unlinks the actor, 1 runs `Actor01500_Fn02C34`
/// and spawns an effect at frame 15, 3 frees the model's buffers once
/// `field_37E` passes 1 and 4 unlinks on its first frame; 1, 3 and 4 move to
/// 2 once `field_362` runs out, and 2 destroys the enemy.
void Actor01500_Fn01DF0(GpEnemy* arg0, Task* arg1)
{
    VECTOR           pos;
    Actor101500Work* work;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   sub;

    model = arg1->extra;
    work  = arg1->work;
    coord = model->coords;
    switch (D_801153F4) {
        case 0:
            break;
        case 1:
            sub = &coord[1];
            goto update;
        case 2:
            model->flags     = 0x80;
            arg0->node.flags = 1;
            return;
    }
    switch (work->field_35C) {
        case 0:
            work->field_368 = 0x1000;
            work->field_32C = coord->coord;
            arg0->recs      = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->field_1DC);
            Gp_UnlinkObj(&work->field_244);
            Gp_UnlinkObj(&work->field_2DC);
            Gp_SetLightMode((GpObj4C*)arg0, 1);
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xF);
            work->field_362 = 0;
            work->field_35C = 1;
            if (work->field_37E != 0) {
                model->flags    = 0x80;
                work->field_35C = 3;
            }
            break;
        case 1:
            Actor01500_Fn02C34(arg1);
            work->field_362++;
            if (work->field_362 == 10) {
                model->flags = 2;
            }
            if (work->field_362 == 15) {
                Gp_SpawnEff(0x600A5, coord, 2, NULL);
            }
            if (work->field_362 >= 60) {
                work->field_35C = 2;
            }
            break;
        case 2:
            Gp_DestroyEnemy(arg0, arg1);
            return;
        case 3:
            if (work->field_37E != 0) {
                if (work->field_37E >= 2) {
                    work->field_37E = 0;
                    Tmd_FreeBuffers(model);
                    model->flags |= 4;
                    Actor01500_Fn01AB0(arg1);
                } else {
                    work->field_37E++;
                }
            }
            work->field_362++;
            if (work->field_362 >= 60) {
                work->field_35C = 2;
            }
            break;
        case 4:
            if (work->field_362 == 0) {
                Gp_UnlinkNode(&arg0->node);
                Gp_UnlinkObj(&work->field_1DC);
                Gp_UnlinkObj(&work->field_244);
                Gp_UnlinkObj(&work->field_2DC);
                Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xF);
            }
            work->field_362++;
            if (work->field_362 >= 61) {
                work->field_35C = 2;
            }
            break;
    }
    sub = ((TmdObject*)arg1->extra)->coords;
    sub = &sub[1];
update:
    pos.vx = sub->workm.t[0];
    pos.vy = sub->workm.t[1];
    pos.vz = sub->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
}

void Actor01500_Fn020D8(Task* arg0)
{
    u8*              head;
    VECTOR3*         stk;
    VECTOR3*         vec;
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s32              dy;
    s32              ady;
    u16              val;
    u16              val2;
    u16*             tbl;
    s32              off;
    s32              delay;

    head              = *(u8**)0x1F8003FC;
    stk               = (VECTOR3*)(head - 0x10);
    *(u8**)0x1F8003FC = (u8*)stk;
    vec               = stk;
    work              = arg0->work;
    coord             = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_35C) {
        case 0:
            off = work->field_364 + 800;
            dy  = Actor01500_D0A092 - off - coord->coord.t[1];
            ady = abs(dy);
            if (ady < 30 || --work->field_362 <= 0) {
                work->field_35C = 1;
            } else {
                work->field_366 = dy > 0 ? 30 : -30;
            }
            vec->vx                = Actor01500_D0A090.vx - coord->coord.t[0];
            vec->vy                = 0;
            vec->vz                = Actor01500_D0A090.vz - coord->coord.t[2];
            work->field_372        = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376        = 100;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
            break;
        case 1:
            work->field_366 = 0;
            work->field_360 = 0;
            work->field_36C = 0;
            if (--work->field_362 < 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                val             = Actor01500_D09FE8[(Gp_LcgState >> 16) & 0xF];
                work->field_352 = 6;
                work->field_35C = 2;
                work->field_35E = val;
            }
            ((VECTOR3*)(head - 0x10))->vx = Actor01500_D0A090.vx - coord->coord.t[0];
            stk->vy                       = 0;
            stk->vz                       = Actor01500_D0A090.vz - coord->coord.t[2];
            work->field_372               = ratan2((s16)((VECTOR3*)(head - 0x10))->vx, (s16)stk->vz) & 0xFFF;
            work->field_376               = 100;
            break;
        case 2:
            work->field_360  = 200;
            work->field_35E -= 200;
            if ((s16)work->field_35E < 0) {
                tbl             = Actor01500_D09FC8;
                work->field_352 = 5;
                val2            = tbl[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_35C = 1;
                work->field_362 = val2;
            }
            break;
    }
    if (D_80073B8C->t[0] > Actor01500_D0A098.vx && D_80073B8C->t[2] < Actor01500_D0A098.vz) {
        work->field_35A = 3;
        delay           = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F) + 60;
        work->field_352 = 5;
        work->field_380 = 15;
        work->field_35C = 0;
        work->field_34C = 0x400F0002;
        work->field_358 = 1;
        work->field_37A = 1;
        work->field_362 = delay;
        work->field_364 = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1FF;
        Gp_ArmStateF0(1);
    }
    *(u8**)0x1F8003FC += 0x10;
}

/// Runs the task's current state handler from `Actor01500_D00004`, copying
/// the table onto the stack before the call.
void Actor01500_Fn02428(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor01500_D00004;
    sp.funcs[task->state](task->spawnArg2, task);
}

/// Per-frame handler. Scene mode 1 only recolours and shadows the actor and
/// mode 2 hides it; otherwise it applies pending hit reactions and contacts,
/// hands off to the teardown state once `field_378` is raised in the
/// states that allow it, runs the behaviour state, turns, moves, animates
/// and voices the actor and rebuilds its root coordinate.
void Actor01500_Fn02484(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Actor101500Work* work;
    s32              state;
    s32              one;

    obj   = arg1->extra;
    state = D_801153F4;
    work  = arg1->work;
    coord = obj->coords;
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
    obj->flags       = 0;
    arg0->node.flags = 0;
    goto default_body;
case2:
    obj->flags       = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor01500_Fn025C8(arg1);
    }
    Actor01500_Fn004EC(arg1);
    if (work->field_378 != 0) {
        if ((work->field_35A == 5) || (work->field_37E != 0)) {
            work->field_35A = 8;
            work->field_35C = 0;
            arg1->state     = 2;
        }
    }
    Actor01500_Fn026D8(arg1);
    if (work->field_376 != 0) {
        Actor01500_Fn01838(arg1);
    }
    Actor01500_Fn01988(arg1);
    Actor01500_Fn02958(arg1);
    Actor01500_Fn02A1C(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor01500_Fn02B14(arg1);
    Actor01500_Fn02B70(arg1);
}

/// Flag bits 0/1 of `field_4C` knock the actor into pose 13; bits 2/3 tick
/// the damage-over-time effect and apply each hit.
void Actor01500_Fn025C8(Task* actor)
{
    GpEnemy*         enemy;
    Actor101500Work* work;
    s32              damage;
    u8               flags;

    enemy = actor->spawnArg2;
    flags = enemy->reactionFlags;
    work  = actor->work;
    if (flags & 1) {
        enemy->reactionFlags = flags & 0xFE;
        if (work->field_358 != 2) {
            work->field_35A = 4;
        }
        work->field_362 = 0;
        work->field_352 = 13;
        work->field_34C = 0;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= 0xFD;
        if (work->field_358 != 2) {
            work->field_35A = 4;
        }
        work->field_362 = 0;
        work->field_352 = 13;
        work->field_34C = 0;
    }
    if (enemy->reactionFlags & 0xC) {
        damage = Gp_TickObjFlag4((GpObj5C*)enemy);
        if (damage != 0) {
            Actor01500_Fn00AFC(actor, damage);
            func_800DA6E8(&enemy->node, damage, 0);
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }
}

/// Runs the behaviour state `field_35A` selects. State 8 has no handler.
void Actor01500_Fn026D8(Task* arg0)
{
    switch (((Actor101500Work*)arg0->work)->field_35A) {
        case 0:
            Actor01500_Fn00CA4(arg0);
            break;
        case 1:
            Actor01500_Fn027B0(arg0);
            break;
        case 2:
            Actor01500_Fn00FC4(arg0);
            break;
        case 3:
            Actor01500_Fn011B0(arg0);
            break;
        case 4:
            Actor01500_Fn0288C(arg0);
            break;
        case 5:
            Actor01500_Fn015DC(arg0);
            break;
        case 6:
            Actor01500_Fn028B0(arg0);
            break;
        case 7:
            Actor01500_Fn01708(arg0);
            break;
        case 9:
            Actor01500_Fn020D8(arg0);
            break;
    }
}

/// Counts `field_362` up, raising the state-F0 flags at frame 60; from frame 90
/// it switches to pose 7 (8 when `field_36E` is set) and reloads the counter
/// with a random delay.
void Actor01500_Fn027B0(Task* actor)
{
    Actor101500Work* work = actor->work;
    s16              pose;
    u32              rnd;
    u16              val;

    if (++work->field_362 == 60) {
        Gp_SetStateF0Byte3(1);
        Gp_SetStateF0Bit(1);
    }
    pose = 7;
    if (work->field_362 >= 90) {
        work->field_35A = 2;
        if (work->field_36E != 0) {
            pose = 8;
        }
        work->field_352 = pose;
        rnd             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rnd;
        val             = Actor01500_D09FC8[(rnd >> 16) & 0xF];
        work->field_358 = 1;
        work->field_37A = 1;
        work->field_34C = 0x400F0002;
        work->field_380 = 15;
        work->field_362 = val;
    }
}

/// Stagger state, entered from a hit reaction or at low hit points: a slow
/// forward drift (`field_360`) with a climb of 0x80 a frame (`field_366`),
/// the second collision object's centre moved to (0, -300, 0).
void Actor01500_Fn0288C(Task* arg0)
{
    Actor101500Work* work = arg0->work;

    work->field_360        = 0x14;
    work->field_366        = 0x80;
    work->field_244.pos.vy = -300;
    work->field_244.pos.vz = 0;
}

/// Countdown pose. Requests pose 9 and clears the move state each frame; when
/// `field_362` runs out it switches to pose 7 and reloads the countdown from a
/// `Gp_LcgState` draw into `Actor01500_D09FC8`.
void Actor01500_Fn028B0(Task* actor)
{
    Actor101500Work* work = actor->work;
    u32              rnd;
    u16              val;
    u16*             tbl;

    work->field_352 = 9;
    work->field_34C = 0;
    work->field_360 = 0;
    work->field_366 = 0;
    if (--work->field_362 == 0) {
        tbl             = Actor01500_D09FC8;
        work->field_358 = 1;
        work->field_35A = 2;
        work->field_352 = 7;
        work->field_35C = 0;
        rnd             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rnd;
        val             = tbl[(rnd >> 16) & 0xF];
        work->field_36E = 0;
        work->field_34C = 0x400F0002;
        work->field_380 = 15;
        work->field_362 = val;
    }
}

/// Animation tick. When the pose the actor asks for differs from the one its
/// slots were last queued for, every slot is re-seeded from the per-state
/// animation id table and the frame counter is cleared; while the two agree
/// each slot is ticked and the frame counter accumulates the slot index.
void Actor01500_Fn02958(Task* arg0)
{
    Actor101500Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    i    = 1;
    if ((s16)work->field_352 != work->field_354) {
        work->field_354 = work->field_352;
        work->field_356 = 0;
        value           = Actor01500_D0A050[(s16)work->field_352];
        for (; i < 7; i++) {
            func_800B4114(work, i, (s16)work->field_352, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_356 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 7);
    }
}

/// Voice tick: while `field_34C` holds a sound id, plays it every third
/// animation frame, tagged with the placement number, and once `field_380`
/// runs out switches to the 0x400F0003 cue.
void Actor01500_Fn02A1C(Task* arg0)
{
    s16              timer;
    s32              soundId;
    s32              objectSoundId;
    s32              pan;
    GsCOORDINATE2*   object;
    Actor101500Work* work;

    work          = arg0->work;
    objectSoundId = work->field_34C;
    object        = ((TmdObject*)arg0->extra)->coords;
    if (objectSoundId != 0) {
        if ((s16)((s16)work->field_356 % 3) == 1) {
            soundId = objectSoundId | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
            pan     = (s8)Gp_GetObjPan(object);
            SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
        }
        if (work->field_380 > 0) {
            timer           = (u16)work->field_380 - 1;
            work->field_380 = timer;
            if ((timer << 0x10) <= 0) {
                work->field_34C = 0x400F0003;
                work->field_380 = 0;
            }
        }
    }
}

/// Hands `Gp_UpdateActorColor` the world position of the model's second
/// coordinate, with no blend parameters.
void Actor01500_Fn02B14(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = &((TmdObject*)arg0->extra)->coords[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

/// Ground shadow for the actor: carves a `VECTOR3` off the scratchpad and fills
/// it from the root coordinate's world translation - straight out of `workm.t`
/// in state 5, otherwise from the hit point `func_800EA1A8` finds casting a
/// ray down. The shade passed to `Gp_DrawEffGroundQuad` is `0x80` in state 5,
/// otherwise `func_800EA318`'s reading of the ray's drop.
void Actor01500_Fn02B70(Task* arg0)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR3*         vec;
    void*            head;
    s16              hit;

    head                = *(void**)0x1F8003FC;
    work                = arg0->work;
    coord               = ((TmdObject*)arg0->extra)->coords;
    *(void**)0x1F8003FC = (u8*)head - 0x10;
    vec                 = (VECTOR3*)((u8*)head - 0x10);
    if (work->field_35A != 5) {
        hit = func_800EA1A8((VECTOR3*)coord->workm.t, vec);
        if (hit != 0) {
            Gp_DrawEffGroundQuad(vec, 0x200, func_800EA318(0x200, 0x80, hit));
        }
    } else {
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0x80);
    }
    *(void**)0x1F8003FC += 0x10;
}

/// Death shrink: restores the root coordinate from the matrix `field_32C`
/// saved when the death sequence began and squashes it along Y by
/// `field_368`, which winds down by 0x50 a frame until it reaches 0x200. The
/// scale is applied through an identity rotation carved off the scratchpad,
/// `ScaleMatrix` and `MulMatrix`, and `flg` is cleared so the coordinate's work
/// matrix is rebuilt.
void Actor01500_Fn02C34(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor101500Work*            work;

    head                    = *(MATRIX**)G_SCRATCH_HEAD;
    work                    = arg0->work;
    scratch                 = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)G_SCRATCH_HEAD = scratch;
    coord                   = ((TmdObject*)arg0->extra)->coords;
    if (work->field_368 >= 0x201) {
        work->field_368 = (u16)work->field_368 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_368;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_32C;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg             = 0;
    *(u8**)G_SCRATCH_HEAD += 0x30;
}
