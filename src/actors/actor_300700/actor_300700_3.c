#include "common.h"

#include "actors/actor_300700.h"
#include "actors/actor_300700_spawn.h"
#include "actors/actor_300700_spawn2.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "psyq/inline_c.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_300700_80161E24;
extern GpEnemyTaskFuncTable3 D_actor_300700_80161E30;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_300700_801637E4(Actor300700* arg0);
void func_actor_300700_80164794(Actor300700* arg0);
void func_actor_300700_80163D64(Actor300700* arg0);
void func_actor_300700_80164070(Actor300700* arg0);
void func_actor_300700_801643D0(Actor300700* arg0);
void func_actor_300700_801645F8(Actor300700* arg0);
void func_actor_300700_80164E38(Actor300700* arg0, Actor300700Obj2C* arg1, s32 arg2);
void func_actor_300700_80164F68(Actor300700* arg0);
void func_actor_300700_80165000(Actor300700* arg0);
void func_actor_300700_801650C0(Actor300700* arg0);
void func_actor_300700_801651A0(Actor300700* arg0);
void func_actor_300700_80165230(Actor300700* arg0);
void func_actor_300700_801652F4(Actor300700* arg0);
void func_actor_300700_8016534C(Actor300700* arg0);
void func_actor_300700_8016539C(Actor300700* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_800B4114(Actor300700Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

void func_actor_300700_801622B4(Actor300700* arg0);
void func_actor_300700_8016252C(Actor300700* arg0);
void func_actor_300700_801626C0(Actor300700* arg0);
void func_actor_300700_801628C8(Actor300700* arg0);
void func_actor_300700_801633B8(Actor300700* arg0);
void func_actor_300700_80162EFC(Actor300700* arg0);

extern u8  D_801153F4;
extern s8  D_80115408;
extern s32 Gp_LcgState;
/// `D_80073B8C` is the camera-target matrix positions are measured from.
extern MATRIX* D_80073B8C;
/// Per-variant base speed, indexed by `GpAreaPlace::rowIndex`.
extern u16 D_actor_300700_80165B78[];

/// Spawn pair table packed by `Gp_PackPair`, and the enemy record whose
/// `pairTable` points at it and whose `hpMax` seeds the enemy's `field_40`.
extern GpU16Pair  D_actor_300700_80165B64;
extern GpPairSrcE D_actor_300700_80165B68;
/// Pose source handed to `func_800B3F84` as its animation data record.
extern u32 D_actor_300700_80165B94;

/// The second variant's pair of the same kind, and its pose source.
extern GpU16Pair  D_actor_300700_80169328;
extern GpPairSrcE D_actor_300700_8016932C;
extern u32        D_actor_300700_801693B8;

/// Per-`field_F` roll thresholds and the timer tables picked by the second
/// roll, for the two idle transitions of `func_actor_300700_80163D64`.
extern s16 D_actor_300700_8016933C[];
extern u16 D_actor_300700_8016934C[];
extern s16 D_actor_300700_8016936C[];
extern u16 D_actor_300700_8016937C[];
extern s16 D_actor_300700_8016939C[];

void func_actor_300700_80165230(Actor300700* arg0)
{
    Actor300700Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if ((s16)work->field_37E != work->field_380) {
        work->field_380 = work->field_37E;
        work->field_382 = 0;
        value           = D_actor_300700_801693E4[(s16)work->field_37E];
        for (; i < 7; i++) {
            func_800B4114(work, i, (s16)work->field_37E, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_382 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 7);
    }
}

/// Updates the actor's lighting colour from the world position of its model
/// root, with both extra arguments zero.
void func_actor_300700_801652F4(Actor300700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg0->field_20, &vec, 0, 0);
}

void func_actor_300700_8016534C(Actor300700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x1C0, 0x80);
}

void func_actor_300700_8016539C(Actor300700* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor300700Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_390 >= 0x201) {
        work->field_390 = (u16)work->field_390 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_390;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_340;
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
