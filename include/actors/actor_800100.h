#ifndef ACTOR_800100_H
#define ACTOR_800100_H

#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// 0x5C-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_80166514`: the `GsCOORDINATE2` it hands to
/// `Gp_PlaceCoordOffset` / `func_actor_800100_801668C0`, the `rot` offset
/// applied to it, and the angle `func_actor_800100_8016709C` returns.
typedef struct _Actor800100PlaceScratch {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ SVECTOR       rot;
    /* 0x58 */ u16           angle;
    /* 0x5A */ byte          pad_5A[2];
} Actor800100PlaceScratch;
STATIC_ASSERT_SIZEOF(Actor800100PlaceScratch, 0x5C);

/// 0x20-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_80164710`: the lock position `Gp_GetLockPos` fills (also
/// the `VECTOR3` handed to `func_80103C74`), and the `rot` vector above it
/// whose `vx`/`vz` `func_80103D8C` measures.
typedef struct _Actor800100LockScratch {
    /* 0x00 */ VECTOR3 lock;
    /* 0x0C */ byte    pad_C[4];
    /* 0x10 */ VECTOR3 rot;
    /* 0x1C */ byte    pad_1C[4];
} Actor800100LockScratch;
STATIC_ASSERT_SIZEOF(Actor800100LockScratch, 0x20);

void func_actor_800100_80163C04(GpActorWork* arg0);
void func_actor_800100_80163D54(GpActorWork* arg0);
void func_actor_800100_801655C0(GpActorWork* arg0);
void func_actor_800100_80165630(GpActorWork* arg0);
void func_actor_800100_80165664(GpActorWork* arg0);
void func_actor_800100_801656C8(GpActorWork* arg0);
void func_actor_800100_801656F4(GpActorWork* arg0);
void func_actor_800100_80165720(GpActorWork* arg0);
void func_actor_800100_80165748(GpActorWork* arg0);
void func_actor_800100_801657D8(GpActorWork* arg0);
void func_actor_800100_80165930(GpActorWork* arg0);
void func_actor_800100_801659EC(GpActorWork* arg0);
void func_actor_800100_80165DE8(GpActorWork* arg0);
void func_actor_800100_80166DD0(GpActorWork* arg0);
void func_actor_800100_80166DF0(GpActorWork* arg0);
void func_actor_800100_80166E14(GpActorWork* arg0);
void func_actor_800100_80166E94(GpActorWork* arg0, s32 arg1);
void func_actor_800100_80166EE8(GpActorWork* arg0);
void func_actor_800100_8016666C(GsCOORDINATE2* arg0, s16 arg1);
void func_actor_800100_801668C0(GsCOORDINATE2* arg0);
s32  func_actor_800100_80166B40(GpRec18* arg0, GsCOORDINATE2* arg1, GsCOORDINATE2* arg2);
s32  func_actor_800100_8016709C(GsCOORDINATE2* arg0, GpRec18* arg1, GpRec18* arg2);

#endif
