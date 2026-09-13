#ifndef ACTORS_SHARED_80135098_H
#define ACTORS_SHARED_80135098_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// A `MATRIX` plus the word-wise view used to splat an identity rotation before
/// `ScaleMatrix` scales it: five aligned stores instead of nine halfword ones,
/// each word holding two adjacent `m[][]` entries. Same shape as
/// `ActorShared80135b58Mat`, which the sibling shared body borrows.
typedef union ActorShared80135098Mat {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} ActorShared80135098Mat;
STATIC_ASSERT_SIZEOF(ActorShared80135098Mat, 0x20);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `ActorsShared80135098`: an
/// identity `mat` scaled by `scale`, then multiplied into the actor's attach
/// coordinate.
typedef struct ActorShared80135098Scratch {
    /* 0x00 */ ActorShared80135098Mat mat;
    /* 0x20 */ VECTOR                 scale;
} ActorShared80135098Scratch;
STATIC_ASSERT_SIZEOF(ActorShared80135098Scratch, 0x30);

/// Partial view of the actor's work block behind `field_1C` (the block the
/// overlay's own `ActorsShared80134cfcWork` and `func_actor_102400_80134318`
/// also reach into). `field_100` is the matrix mirrored out of the attach
/// coordinate by `func_actor_102400_80133EAC`, which divides its fixed-point
/// constant by `field_12A` - the scale `ActorsShared80135098` applies on the
/// coordinate's Y axis.
typedef struct ActorShared80135098Work {
    /* 0x000 */ byte   pad_0[0x100];
    /* 0x100 */ MATRIX field_100;
    /* 0x120 */ byte   pad_120[0xA];
    /* 0x12A */ s16    field_12A;
} ActorShared80135098Work;

typedef struct ActorShared80135098Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80135098Obj2C;

typedef struct ActorShared80135098 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80135098Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80135098Obj2C* field_2C;
} ActorShared80135098;

void ActorsShared80135098(ActorShared80135098* arg0);

#endif
