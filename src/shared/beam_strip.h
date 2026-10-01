/* Beam strip: a textured POLY_FT4 stretched between an effect coordinate's
 * world position and an endpoint - both projected, the strip dropped if
 * either fails - its half-width `width * 23 / otz` turned square to the
 * strip's screen angle. `cell` picks one of four strips on the page (bit 0
 * the half, bit 1 the row). The M4A1 Hammer's shock trail and the M.I.S.T.
 * shooting gallery's tracer.
 *
 * BEAM_STRIP_OTZ_BIAS (default 1) sorts the strip a slot behind each
 * surviving projection; the gallery's tracer sorts at the first point (0).
 */
#ifndef SRC_SHARED_BEAM_STRIP_H
#define SRC_SHARED_BEAM_STRIP_H

#include "common.h"
#include "main/coord.h"

/// 0x20-byte scratch block `beamStripDraw` carves off the scratch stack.
///
/// `vec` is the effect coordinate's world position (`workm.t`) truncated to
/// s16; it and the caller's endpoint `SVECTOR` are projected by one `RTPS`
/// each, filling `sxy0` / `sxy1` through `gte_stsxy`. `flag` is `gte_stflg` of
/// whichever projection just ran - both are tested, so an off-screen endpoint
/// drops the whole strip - and `otz` is `gte_stszotz` of the first point,
/// bumped once per surviving projection (unless BEAM_STRIP_OTZ_BIAS is 0) so it serves as both the divisor of
/// the strip's half-width and the OT index the primitive is queued at. `dx` /
/// `dy` are that half-width rotated by `(width * 23 / otz) * rsin|rcos(angle)
/// >> 12`, applied once at the strip's own screen angle and once at 90 degrees
/// to it to give the `POLY_FT4` its four corners.
typedef struct _BeamStripScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ DVECTOR sxy0;
    /* 0x1C */ DVECTOR sxy1;
} BeamStripScratch;
STATIC_ASSERT_SIZEOF(BeamStripScratch, 0x20);

static void beamStripDraw(GfxCoord* coord, SVECTOR* end, s32 cell, s16 width);

#endif /* SRC_SHARED_BEAM_STRIP_H */
