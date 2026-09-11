#ifndef ACTORS_SHARED_8016A538_H
#define ACTORS_SHARED_8016A538_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// A `MATRIX` plus the word-wise view `ActorsShared8016a538` uses to splat an
/// identity rotation before `RotMatrixX` overwrites it: five aligned stores
/// instead of nine halfword ones (the same shape as `HyperMat`).
typedef union ActorsShared8016a538Mat {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} ActorsShared8016a538Mat;
STATIC_ASSERT_SIZEOF(ActorsShared8016a538Mat, 0x20);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`): the angle is a `long`,
/// so a negated angle is passed without re-truncation to 16 bits.
void func_8004BFF8(s32 angle, MATRIX* matrix);

/// Rebuild the model root's rotation: pitch about X by a sine sway driven by
/// `field_442`, then turn by the heading `field_7A`, and copy the 3x3 into the
/// root coordinate. When `field_41E` is 1, latch that pitch into `field_434`,
/// clear the flag and three motion halfwords, and advance `field_422`. Shared
/// by `actor_341700` and `actor_342400`.
void ActorsShared8016a538(Task* arg0);

#endif
