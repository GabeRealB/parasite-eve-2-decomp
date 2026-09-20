#ifndef ACTORS_SHARED_80136184_H
#define ACTORS_SHARED_80136184_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

/// Projected endpoints in the work block shared by actor_402200 and
/// actor_403900, using the same offsets as Actor402200Work.
typedef struct ActorShared80136184Work {
    /* 0x000 */ byte pad_0[0x6FC];
    /* 0x6FC */ s16  field_6FC[2];
    /* 0x700 */ s16  field_700[2];
    /* 0x704 */ s16  field_704[2];
    /* 0x708 */ byte pad_708[0x14];
} ActorShared80136184Work;
STATIC_ASSERT_SIZEOF(ActorShared80136184Work, 0x71C);

typedef struct ActorShared80136184 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80136184Work* field_1C;
} ActorShared80136184;

/// Scratch block for the normalized screen direction, interpolated depth,
/// six vertex pairs and endpoint increments of the actor's red trail.
typedef struct ActorShared80136184Scratch {
    /* 0x00 */ VECTOR  dir;
    /* 0x10 */ SVECTOR norm;
    /* 0x18 */ s32     z;
    /* 0x1C */ s32     dz;
    /* 0x20 */ u16     x[6];
    /* 0x2C */ u16     y[6];
    /* 0x38 */ s16     dx;
    /* 0x3A */ s16     dy;
} ActorShared80136184Scratch;
STATIC_ASSERT_SIZEOF(ActorShared80136184Scratch, 0x3C);

/// Each overlay binds this name to its own two four-vertex index rows.
extern s16 ActorsShared80136184Indices[2][4];

/// Draws eight trail segments with shaded quads, a center line and a tpage.
void ActorsShared80136184(ActorShared80136184* arg0);

#endif
