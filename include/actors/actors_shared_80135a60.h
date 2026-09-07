#ifndef ACTORS_SHARED_80135A60_H
#define ACTORS_SHARED_80135A60_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// 0x34-byte block taken from `G_SCRATCH_HEAD`: a `MATRIX` plus the uniform
/// `VECTOR` handed to `ScaleMatrix` and the yaw stored before `Gfx_RotMatrixY`.
typedef struct ActorShared80135a60Scratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} ActorShared80135a60Scratch;
STATIC_ASSERT_SIZEOF(ActorShared80135a60Scratch, 0x34);

void ActorsShared80135a60(GsCOORDINATE2* coord, s16 scale);

#endif
