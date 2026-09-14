#ifndef ACTORS_SHARED_80134084_H
#define ACTORS_SHARED_80134084_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80134084Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80134084Obj2C;

/// 0x154-byte work block the actor's constructor allocates and hangs off
/// `ActorShared80134084.field_1C`. `field_13A` is the per-frame turn step and
/// `field_144` / `field_146` the current and wanted yaw.
typedef struct ActorShared80134084Work {
    /* 0x000 */ byte pad_0[0x13A];
    /* 0x13A */ s16  field_13A;
    /* 0x13C */ byte pad_13C[0x8];
    /* 0x144 */ s16  field_144;
    /* 0x146 */ u16  field_146;
    /* 0x148 */ byte pad_148[0xC];
} ActorShared80134084Work;
STATIC_ASSERT_SIZEOF(ActorShared80134084Work, 0x154);

typedef struct ActorShared80134084 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80134084Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80134084Obj2C* field_2C;
} ActorShared80134084;

/// The 0x18-byte scratchpad block the yaw step and its `RotMatrix` argument
/// live in.
typedef struct ActorShared80134084RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} ActorShared80134084RotScratch;
STATIC_ASSERT_SIZEOF(ActorShared80134084RotScratch, 0x18);

/// Turns the actor towards the yaw in `field_146` by at most `field_13A` per
/// frame, then rebuilds the part coordinate's matrix from the resulting angle.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared80134084(ActorShared80134084* arg0);

#endif
