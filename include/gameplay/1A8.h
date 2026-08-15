#ifndef GAMEPLAY_1A8_H
#define GAMEPLAY_1A8_H

#include "common.h"

#include "main/task.h"

/// Overlay of `GsCOORDINATE2` at `GameActorExt.field_8`. `field_18` /
/// `field_20` are the low 16 bits of `coord.t[0]` / `coord.t[2]` (world X/Z).
typedef struct _GpCoordXZ {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  field_18;
    /* 0x1A */ byte pad_1A[6];
    /* 0x20 */ u16  field_20;
} GpCoordXZ;
STATIC_ASSERT_SIZEOF(GpCoordXZ, 0x22);

/// SVECTOR layout with unsigned X/Z so `func_800AEE28` emits `lhu`.
typedef struct _GpPosXZ {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 pad_2;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad_6;
} GpPosXZ;
STATIC_ASSERT_SIZEOF(GpPosXZ, 8);

s32 func_800AEE28(Task* arg0, GpPosXZ* arg1);

#endif // GAMEPLAY_1A8_H
