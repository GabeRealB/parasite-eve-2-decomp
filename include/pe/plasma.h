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

/// Projects `arg0`'s world position and queues sixteen gouraud `POLY_G4`
/// wedges forming a ring around it. `arg1` is the inner half-extent and
/// `arg2` the extra outer width; the on-screen radii are
/// `(s16)arg1 * 64 / (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`.
/// `rgb` tints the inner edge so each wedge fades to a black outer rim.
void func_plasma_8012FB10(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

#endif /* PE_PLASMA_H */
