#ifndef ACTOR_135600_H
#define ACTOR_135600_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Word-wise view of a `MATRIX` used to splat an identity rotation: five
/// aligned stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries.  The same shape `ActorsShared801639a8MatWords` has.
typedef struct Actor135600MatrixWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor135600MatrixWords;

typedef union Actor135600Matrix {
    MATRIX                 mat;
    Actor135600MatrixWords ident;
} Actor135600Matrix;
STATIC_ASSERT_SIZEOF(Actor135600Matrix, 0x20);

/// The world coordinate every `GsCOORDINATE2` chain is rooted at.
extern GsCOORDINATE2 Gfx_ViewCoord;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Recomputes `coord`'s world matrix (`Gp_UpdateCoord`) and then projects a
/// rotating pair of offsets through it, returning the signed `ratan2` of the
/// difference between the two projections - the actor's screen-space angle.
/// `arg1` selects how far the pair is rotated: it scales by 70/4096 of a
/// revolution, so the 0x800 the tick handler latches on a hit is 35 degrees.
s32 func_actor_135600_80131E68(GsCOORDINATE2* coord, s16 arg1);

#endif // ACTOR_135600_H
