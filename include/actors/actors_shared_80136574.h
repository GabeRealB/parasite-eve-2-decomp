#ifndef ACTORS_SHARED_80136574_H
#define ACTORS_SHARED_80136574_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// A `MATRIX` plus the word-wise view `ActorsShared80136574` uses to splat an
/// identity rotation: five aligned stores instead of nine halfword ones, each
/// word holding two adjacent `m[][]` entries.
typedef union ActorShared80136574Mat {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} ActorShared80136574Mat;
STATIC_ASSERT_SIZEOF(ActorShared80136574Mat, 0x20);

/// 0x30-byte block borrowed from `G_SCRATCH_HEAD` by
/// `ActorsShared80136574`: an identity `mat` scaled by `scale`, then
/// multiplied into the actor's attach coordinate.
typedef struct ActorShared80136574Scratch {
    /* 0x00 */ ActorShared80136574Mat mat;
    /* 0x20 */ VECTOR                 scale;
} ActorShared80136574Scratch;
STATIC_ASSERT_SIZEOF(ActorShared80136574Scratch, 0x30);

typedef struct ActorShared80136574Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80136574Obj2C;

typedef struct ActorShared80136574 {
    /* 0x00 */ byte                      pad_0[0x2C];
    /* 0x2C */ ActorShared80136574Obj2C* field_2C;
} ActorShared80136574;

void ActorsShared80136574(ActorShared80136574* arg0, MATRIX* arg1, s16 arg2, s32 arg3);

#endif
