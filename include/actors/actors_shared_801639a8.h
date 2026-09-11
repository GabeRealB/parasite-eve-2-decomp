#ifndef ACTORS_SHARED_801639A8_H
#define ACTORS_SHARED_801639A8_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Word-wise view of a `MATRIX` that `ActorsShared801639a8` uses to splat an
/// identity rotation: five aligned stores instead of nine halfword ones, each
/// word holding two adjacent `m[][]` entries.
typedef struct ActorsShared801639a8MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} ActorsShared801639a8MatWords;

typedef union ActorsShared801639a8Mat {
    MATRIX                       mat;
    ActorsShared801639a8MatWords ident;
} ActorsShared801639a8Mat;
STATIC_ASSERT_SIZEOF(ActorsShared801639a8Mat, 0x20);

/// Turn model parts 5, 4 and 3 about Y by a third of `field_424` each: read
/// each part's rotation back as Euler angles, add to the yaw, rebuild the 3x3
/// and mark the coordinate dirty. Shared by `actor_341700` and `actor_342400`.
void ActorsShared801639a8(Task* arg0);

#endif
