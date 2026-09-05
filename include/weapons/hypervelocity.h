#ifndef WEAPONS_HYPERVELOCITY_H
#define WEAPONS_HYPERVELOCITY_H

#include "common.h"

#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "main/session.h"

/// A `MATRIX` plus the word-wise view this overlay uses to splat an identity
/// rotation before `RotMatrixX` overwrites it: five aligned stores instead of
/// nine halfword ones, each word holding two adjacent `m[][]` entries.
typedef union HyperMat {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} HyperMat;
STATIC_ASSERT_SIZEOF(HyperMat, 0x20);

/// The hypervelocity muzzle/recoil coordinate node, reached through
/// `TmdObject.field_8`. It is the game's `GsCOORDINATE2` variant: `sub`
/// (0x4C) is the parent link, the same convention as `GpCoordExt` in
/// gameplay. This overlay keeps the barrel's pitch in the 16-bit slot at
/// 0x44 (libgs `param`, unused here) and feeds it to `RotMatrixX`.
typedef struct HyperCoord {
    /* 0x00 */ s32            flg;
    /* 0x04 */ HyperMat       coord;
    /* 0x24 */ MATRIX         workm;
    /* 0x44 */ s16            angle;
    /* 0x46 */ s16            field_46;
    /* 0x48 */ s16            field_48;
    /* 0x4A */ byte           pad_4A[2];
    /* 0x4C */ GsCOORDINATE2* sub;
} HyperCoord;
STATIC_ASSERT_SIZEOF(HyperCoord, 0x50);

/// 0x18-byte scratchpad block `func_hypervelocity_8011F724` reserves for one
/// frame of the barrel's recoil kick. `dir` receives the third column of the
/// weapon coordinate matrix from `Gfx_MatrixCol2`; each axis is then scaled by
/// the remaining recoil ticks over a per-tick divisor, negated, and added to
/// the coordinate's translation so the gun rides back along its own barrel.
typedef struct HyperRecoil {
    /* 0x00 */ s32     vx;
    /* 0x04 */ s32     vy;
    /* 0x08 */ s32     vz;
    /* 0x0C */ byte    pad_C[4];
    /* 0x10 */ SVECTOR dir;
} HyperRecoil;
STATIC_ASSERT_SIZEOF(HyperRecoil, 0x18);

/// 0x38 block the round's spawn state allocates with `Mem_Calloc` and parks in
/// `Task::idMap`. It leads with the `GpObj` list node `WeaponsShared8011e4ac`
/// hands back to `Gp_UnlinkObj` on teardown; `rec` is the single-entry
/// `GpRec18` collision table `obj.field_C` points at, and its `field_0` is set
/// to 2 (the last-element bit) instead of going through `Gp_InitRec18Table`.
typedef struct HyperBeam {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[1];
} HyperBeam;
STATIC_ASSERT_SIZEOF(HyperBeam, 0x38);

/// Translation of the round's own coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x240, 0x80)`.
extern SVECTOR D_hypervelocity_8011FB74;

/// Per-particle jitter of the hypervelocity trail, one 8-bit LCG roll each,
/// re-rolled as a block when the round is fired.
extern s16 D_hypervelocity_8012EF0C[16];

#endif
