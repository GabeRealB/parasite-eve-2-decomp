#ifndef PE_FLARE_H
#define PE_FLARE_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// 0x1C-byte scratch block `func_flare_8012F304` takes from `G_SCRATCH_HEAD`.
/// `vec` is the effect coordinate's `workm.t[]` truncated to s16 and projected
/// through `GsWSMATRIX` with one `RTPS`: `flag` is the `gte_stflg` of that
/// projection (a negative value drops the sprite), `otz` its `gte_stszotz` and
/// `sx` / `sy` its `gte_stsxy`. `dx` / `dy` hold the current
/// `(arg2 * 31 / otz) * rsin|rcos(angle) >> 12` offsets that are added to and
/// subtracted from `sx` / `sy` to build the four quad corners; only their low
/// halves are read back. Same layout as the gameplay `GpFxQuadScratch`, which
/// `Gp_DrawFxQuad` builds the same way.
typedef struct FlareQuadScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} FlareQuadScratch;
STATIC_ASSERT_SIZEOF(FlareQuadScratch, 0x1C);

/// Links one frame of the flare sprite at `arg0`'s world position. The
/// position is projected through `GsWSMATRIX` by a single `RTPS` and the quad
/// is dropped when that sets a negative `gte_stflg`. `arg1` picks one of the
/// 0x20-wide texture frames on tpage 0x2A, `arg3` spins the quad and `arg2`
/// sizes it: the corners sit `arg2 * 31 / otz` from the projected centre along
/// `arg3` and `arg3 + 0x400`, so the sprite shrinks with depth. Same shape as
/// the gameplay `Gp_DrawFxQuad`, with the CLUT fixed at 0x4311 instead of
/// picked from `Gp_QuadClutX`.
void func_flare_8012F304(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);

#endif /* PE_FLARE_H */
