#ifndef ACTOR_101500_H
#define ACTOR_101500_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/tmd.h"

/// The actor's animation work area. `field_352` is the pose the actor asks
/// for, `field_354` the pose its slots were last re-queued for and
/// `field_356` the frame count accumulated while the two agree:
/// `func_actor_101500_80134778` re-seeds the slots from the per-state id table
/// when they differ and ticks them while they match.
typedef struct Actor101500Work {
    /* 0x000 */ byte           pad_0[0x14];
    /* 0x014 */ byte           field_14[0x118]; // animation slots, handed to `func_800B3F84`
    /* 0x12C */ byte           field_12C[0x70]; // pose buffer, handed to `func_800B3F84`
    /* 0x19C */ MATRIX         field_19C;       // model colour matrix
    /* 0x1BC */ MATRIX         field_1BC;       // model light matrix
    /* 0x1DC */ GpObj          field_1DC;
    /* 0x1FC */ GpRec18        field_1FC[3];
    /* 0x244 */ GpObj          field_244;
    /* 0x264 */ GpRec18        field_264[5];
    /* 0x2DC */ GpObj          field_2DC;
    /* 0x2FC */ GpRec18        field_2FC[1];
    /* 0x314 */ GsCOORDINATE2* field_314;
    /* 0x318 */ s16            field_318;
    /* 0x31A */ s16            field_31A;
    /* 0x31C */ VECTOR3        field_31C; // position before this frame's step
    /* 0x328 */ byte           pad_328[4];
    /* 0x32C */ MATRIX         field_32C;
    /* 0x34C */ s32            field_34C;
    /* 0x350 */ byte           pad_350[2];
    /* 0x352 */ u16            field_352;
    /* 0x354 */ s16            field_354;
    /* 0x356 */ u16            field_356;
    /* 0x358 */ s16            field_358;
    /* 0x35A */ s16            field_35A;
    /* 0x35C */ s16            field_35C;
    /* 0x35E */ byte           pad_35E[2];
    /* 0x360 */ s16            field_360;
    /* 0x362 */ s16            field_362;
    /* 0x364 */ s16            field_364;
    /* 0x366 */ s16            field_366;
    /* 0x368 */ s16            field_368;
    /* 0x36A */ byte           pad_36A[4];
    /* 0x36E */ s16            field_36E;
    /* 0x370 */ s16            field_370;
    /* 0x372 */ u16            field_372;
    /* 0x374 */ s16            field_374;
    /* 0x376 */ s16            field_376;
    /* 0x378 */ s16            field_378;
    /* 0x37A */ s16            field_37A;
    /* 0x37C */ s16            field_37C;
    /* 0x37E */ s16            field_37E;
    /* 0x380 */ s16            field_380;
    /* 0x382 */ s16            field_382; // spawn variant, `GpAreaPlace.field_1`
} Actor101500Work;

typedef struct Actor101500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor101500Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C; // model; `field_8` is the coordinate array effects spawn against
    /* 0x30 */ s32              field_30;
} Actor101500;

/// Per-state animation id handed to `func_800B4114`, indexed by `field_352`.
extern s16 D_actor_101500_8013BE70[];

/// Fifteen vertical bob offsets cycled by `field_37C` while `field_352` is 5.
extern s16 D_actor_101500_8013BE90[];

/// Sixteen frame counts `func_actor_101500_80133528`, `func_actor_101500_801345D0` and `func_actor_101500_801346D0` reload `field_362` from,
/// picked by a `Gp_LcgState` draw.
extern u16 D_actor_101500_8013BDE8[];

/// 0x18-byte frame allocated on the scratchpad stack; only the `SVECTOR` at
/// +0x10 is used, as the rotation `ActorsShared80133658` hands `RotMatrix`.
typedef struct Actor101500RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor101500RotScratch;
STATIC_ASSERT_SIZEOF(Actor101500RotScratch, 0x18);

extern u32 Gp_LcgState;

void func_800B4114(Actor101500Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Pair packed into the third collision object's `field_18` at spawn.
extern GpU16Pair D_actor_101500_8013BDD4;
/// Enemy stat block; `field_4` seeds the hit points.
extern GpPairSrcE D_actor_101500_8013BDD8;
/// Animation bank handed to `func_800B3F84`.
extern u8 D_actor_101500_8013BE34[];

void func_actor_101500_8013291C(Actor101500* actor, s32 damage);

/// Maximum hit points; `func_actor_101500_8013291C` staggers below 60%.
extern u16 D_actor_101500_8013BDDC;

#endif
