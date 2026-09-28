#ifndef GAMEPLAY_GEOMETRY_H
#define GAMEPLAY_GEOMETRY_H

#include "common.h"

/// A `GpCoord` seen through the low halves of its translation, so each
/// world coordinate loads as an unsigned halfword.
typedef struct _GpCoordPos {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  x;
    /* 0x1A */ byte pad_1A[2];
    /* 0x1C */ u16  y;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ u16  z;
    /* 0x22 */ byte pad_22[2];
} GpCoordPos;
STATIC_ASSERT_SIZEOF(GpCoordPos, 0x24);

/// A 16.16 fixed-point word, read whole or as its fraction and integer halves.
typedef union {
    s32 w;
    struct {
        u16 lo;
        s16 hi;
    } h;
} GpFixed16;

/// 0x10-byte scratch from `G_SCRATCH_HEAD` used by `func_801011D0`.
/// Words at 0/4/8 are the 16.16 deltas from `func_800E0FEC`; if the
/// fractional half is nonzero they are stepped away from zero by 0x10000
/// and the high half is added onto `GpCoord.coord.t[]`.
typedef struct _GpDeltaScratch {
    /* 0x00 */ GpFixed16 vx;
    /* 0x04 */ GpFixed16 vy;
    /* 0x08 */ GpFixed16 vz;
    /* 0x0C */ s32       pad;
} GpDeltaScratch;
STATIC_ASSERT_SIZEOF(GpDeltaScratch, 0x10);

#endif // GAMEPLAY_GEOMETRY_H
