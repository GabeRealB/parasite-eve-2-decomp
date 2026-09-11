#ifndef ACTORS_SHARED_8016BD98_H
#define ACTORS_SHARED_8016BD98_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Word-wise view of a `MATRIX` that `ActorsShared8016bd98` uses to splat an
/// identity rotation: five aligned stores instead of nine halfword ones, each
/// word holding two adjacent `m[][]` entries.
typedef struct ActorsShared8016bd98MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} ActorsShared8016bd98MatWords;

typedef union ActorsShared8016bd98Mat {
    MATRIX                       mat;
    ActorsShared8016bd98MatWords ident;
} ActorsShared8016bd98Mat;
STATIC_ASSERT_SIZEOF(ActorsShared8016bd98Mat, 0x20);

/// Copies `mat_0` into the model root coord and squashes it vertically by the
/// shrinking `field_430` (an identity scaled to (1, field_430, 1) and multiplied
/// in). Frame 16 rotates the light mode to 2; after frame 32 the model is
/// flagged 0x80 and the state advances. Shared by `actor_341700` and
/// `actor_342400`, whose work blocks are the same `ActorsShared80168d3cWork`.
void ActorsShared8016bd98(Task* arg0);

#endif
