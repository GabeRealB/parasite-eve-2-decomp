#ifndef PE_PLASMA_H
#define PE_PLASMA_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// 0x1C-byte scratch block `func_plasma_8012FB10` takes from
/// `G_SCRATCH_HEAD`. `vec` is the effect coordinate's `workm.t[]` at 0x10,
/// projected through `GsWSMATRIX` with one `RTPS`: `flag` is the `gte_stflg`
/// of that projection (a negative value drops the ring), `otz` is its
/// `gte_stszotz` and `sx` / `sy` its `gte_stsxy`. `rOuter` and `rInner` are
/// the two on-screen radii the ring is swept between.
typedef struct PlasmaRingScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     rOuter;
    /* 0x14 */ s32     rInner;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} PlasmaRingScratch;
STATIC_ASSERT_SIZEOF(PlasmaRingScratch, 0x1C);

/// Three 16-entry LCG columns used as per-vertex jitter. `func_plasma_8012EF34`
/// refills them on state 0 by walking an `s16*` across the object so stores
/// land at 0x00 / 0x20 / 0x40; `func_plasma_8012F568` consumes column `arg2`.
typedef struct PlasmaJitter {
    /* 0x00 */ s16 a;
    /* 0x02 */ s16 pad_a[15];
    /* 0x20 */ s16 b;
    /* 0x22 */ s16 pad_b[15];
    /* 0x40 */ s16 c;
    /* 0x42 */ s16 pad_c[15];
} PlasmaJitter;
STATIC_ASSERT_SIZEOF(PlasmaJitter, 0x60);

/// Per-ring radius scale for `func_plasma_8012F568`, indexed by ring number
/// (0..2). `rInner` widens the inner radius (`GpEffWork::field_26`), `rExtra`
/// the outer radius on top of that (`+ field_2A`), and `yOff` raises the
/// inner edge above `field_28`.
typedef struct PlasmaRingScale {
    /* 0x0 */ s16 rInner;
    /* 0x2 */ s16 yOff;
    /* 0x4 */ s16 rExtra;
} PlasmaRingScale;
STATIC_ASSERT_SIZEOF(PlasmaRingScale, 0x6);

extern PlasmaRingScale D_plasma_8012FF34[];

/// `SndEvt` ids for the plasma ring, indexed by `Gp_StateC08.field_0 % 10 - 1`.
extern s32 D_plasma_8012FF48[];

extern PlasmaJitter D_plasma_8012FF54;

/// Draws textured band `arg2` (0..2) of the plasma ring around `arg1`: sixteen
/// `POLY_FT4` wedges between an outer circle of radius
/// `field_26 + rInner + field_2A + rExtra` and an inner one of radius
/// `field_26 + rInner`, the outer ring lifted by `-(field_28 + yOff)`. Both
/// circles are rotated by the coordinate's `workm`, translated by its `t[]`
/// and projected through `GsWSMATRIX`; wedge `i` picks its texture column
/// from `(D_plasma_8012FF54[arg2][i] + field_22) % 6`, and `field_24` sets the
/// brightness. A negative `gte_stflg` on the wedge's first vertex drops it.
/// Works out of a `GpBandScratch` taken from `G_SCRATCH_HEAD`.
void func_plasma_8012F568(GpEffWork* arg0, GsCOORDINATE2* arg1, s32 arg2);

/// Projects `arg0`'s world position and queues sixteen gouraud `POLY_G4`
/// wedges forming a ring around it. `arg1` is the inner half-extent and
/// `arg2` the extra outer width; the on-screen radii are
/// `(s16)arg1 * 64 / (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`.
/// `rgb` tints the inner edge so each wedge fades to a black outer rim.
void func_plasma_8012FB10(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

#endif /* PE_PLASMA_H */
