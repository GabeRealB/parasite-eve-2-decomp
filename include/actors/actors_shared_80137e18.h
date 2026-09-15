#ifndef ACTORS_SHARED_80137E18_H
#define ACTORS_SHARED_80137E18_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// A `MATRIX` plus the word-wise view this body uses to splat an identity
/// rotation: five aligned stores instead of nine halfword ones, each word
/// holding two adjacent `m[][]` entries. The other actor overlays spell the
/// same union `ActorShared80135b58Mat`.
typedef union ActorShared80137e18Mat {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} ActorShared80137e18Mat;
STATIC_ASSERT_SIZEOF(ActorShared80137e18Mat, 0x20);

/// Display object hung off the context's 0x2C slot; `field_8` is the model's
/// per-part `GsCOORDINATE2` array. Same shape as the other actor overlays'
/// display objects.
typedef struct ActorShared80137e18Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80137e18Obj2C;

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::idMap`, which is not a `TaskIdMap` here). `coord` is the extra
/// `GsCOORDINATE2` this body wires as `sub` of the model's second part;
/// `scale` is that node's X/Y/Z in 4096-per-unit fixed point, which
/// `ActorsShared80137ea8` then reads as `field_34E` for the Y component.
/// `field_36A` is 5 when the 0x600A5 spawn is already armed.
typedef struct ActorShared80137e18Work {
    /* 0x000 */ byte          pad_0[0x2EC];
    /* 0x2EC */ GsCOORDINATE2 coord;
    /* 0x33C */ byte          pad_33C[0x10];
    /* 0x34C */ SVECTOR       scale;
    /* 0x354 */ byte          pad_354[0x16];
    /* 0x36A */ s16           field_36A;
    /* 0x36C */ byte          pad_36C[2];
    /* 0x36E */ s16           field_36E;
} ActorShared80137e18Work;
STATIC_ASSERT_SIZEOF(ActorShared80137e18Work, 0x370);

/// Actor context handed to this body: `field_1C` is the work block above and
/// `field_2C` the display object. Same shape as the other actor overlays'
/// contexts.
typedef struct ActorShared80137e18 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80137e18Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80137e18Obj2C* field_2C;
} ActorShared80137e18;

/// Wires the extra coordinate as `sub` of the model's second part, splats an
/// identity rotation into it, clears both nodes' `flg`, and arms the X/Y/Z
/// scale to 0x1000. Unless `field_36A` is 5, also spawns effect 0x600A5 on
/// the model root. Shared verbatim by `actor_107000` and `actor_207000`.
void ActorsShared80137e18(ActorShared80137e18* arg0);

#endif
