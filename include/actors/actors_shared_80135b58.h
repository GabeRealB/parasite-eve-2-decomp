#ifndef ACTORS_SHARED_80135B58_H
#define ACTORS_SHARED_80135B58_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// A `MATRIX` plus the word-wise view `ActorsShared80135b58` uses to splat an
/// identity rotation before `ScaleMatrix` scales it: five aligned stores
/// instead of nine halfword ones, each word holding two adjacent `m[][]`
/// entries.
typedef union ActorShared80135b58Mat {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} ActorShared80135b58Mat;
STATIC_ASSERT_SIZEOF(ActorShared80135b58Mat, 0x20);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `ActorsShared80135b58`: an
/// identity `mat` scaled down by a uniform `scale` of 0x100 (1/16), then
/// multiplied into the model's third coordinate node.
typedef struct ActorShared80135b58Scratch {
    /* 0x00 */ ActorShared80135b58Mat mat;
    /* 0x20 */ VECTOR                 scale;
} ActorShared80135b58Scratch;
STATIC_ASSERT_SIZEOF(ActorShared80135b58Scratch, 0x30);

typedef struct ActorShared80135b58Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80135b58Obj2C;

typedef struct ActorShared80135b58 {
    /* 0x00 */ byte                      pad_0[0x2C];
    /* 0x2C */ ActorShared80135b58Obj2C* field_2C;
} ActorShared80135b58;

void ActorsShared80135b58(ActorShared80135b58* arg0);

#endif
