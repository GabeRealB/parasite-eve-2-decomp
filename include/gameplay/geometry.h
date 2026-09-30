#ifndef GAMEPLAY_GEOMETRY_H
#define GAMEPLAY_GEOMETRY_H

#include "common.h"

/// A signed 16.16 fixed-point value. One whole unit is 0x10000.
///
/// `word` is the full value. `halves.fraction` is the unsigned low half and
/// `halves.integer` the signed high half. Storing the fraction back into
/// `word` keeps that half and clears the integer.
typedef union {
    s32 word;         // Full value
    struct {
        u16 fraction; // Low half, in 1/65536 of a unit
        s16 integer;  // High half, in whole units
    } halves;
} Fixed16;
STATIC_ASSERT_SIZEOF(Fixed16, 4);

/// 0x10-byte scratch from the scratch stack used by `func_801011D0`.
/// Words at 0/4/8 are the 16.16 deltas from `func_800E0FEC`; if the
/// fractional half is nonzero they are stepped away from zero by 0x10000
/// and the high half is added onto `GfxCoord.coord.t[]`.
typedef struct _GpDeltaScratch {
    /* 0x00 */ Fixed16 vx;
    /* 0x04 */ Fixed16 vy;
    /* 0x08 */ Fixed16 vz;
    /* 0x0C */ s32     pad;
} GpDeltaScratch;
STATIC_ASSERT_SIZEOF(GpDeltaScratch, 0x10);

#endif // GAMEPLAY_GEOMETRY_H
