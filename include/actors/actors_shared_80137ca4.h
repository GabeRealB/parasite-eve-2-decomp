#ifndef ACTORS_SHARED_80137CA4_H
#define ACTORS_SHARED_80137CA4_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// A `MATRIX` plus the word-wise view this body uses to splat an identity
/// rotation before `ScaleMatrix` scales it: five aligned stores instead of nine
/// halfword ones, each word holding two adjacent `m[][]` entries. The other
/// actor overlays spell the same union `ActorShared80135b58Mat`.
typedef union ActorShared80137ca4Mat {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} ActorShared80137ca4Mat;
STATIC_ASSERT_SIZEOF(ActorShared80137ca4Mat, 0x20);

/// Work block the carriers hang off the task's `Task::work` slot (0x1C), which
/// is not a `TaskIdMap` here -- the same block `Actor402200Work` describes, seen
/// through the two fields this body reaches: the attach rotation at 0x674 and
/// the per-axis scales at 0x694 that `ScaleMatrix` folds into it.
typedef struct ActorShared80137ca4Work {
    /* 0x000 */ byte   pad_0[0x674];
    /* 0x674 */ MATRIX field_674;
    /* 0x694 */ VECTOR field_694;
} ActorShared80137ca4Work;

/// Display object hung off the actor's 0x2C slot; `field_8` is the model's
/// per-part `GsCOORDINATE2` array, whose root part this body rebuilds.
typedef struct ActorShared80137ca4Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80137ca4Obj2C;

/// Actor context handed to this body: `field_1C` is the work block above and
/// `field_2C` the display object.
typedef struct ActorShared80137ca4 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80137ca4Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80137ca4Obj2C* field_2C;
} ActorShared80137ca4;

/// Rebuilds the root part's coordinate from the work block's attach rotation and
/// scales: the rotation is copied straight into `coord` (+0x04 of the part), an
/// identity is splatted into a 0x20-byte scratchpad matrix and scaled by
/// `field_694`, and the product replaces the part's rotation.
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
void ActorsShared80137ca4(ActorShared80137ca4* arg0);

#endif
