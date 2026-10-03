/* Beam strip: a textured POLY_FT4 stretched between an effect coordinate's
 * world position and an endpoint - both projected, the strip dropped if
 * either fails - its half-width `width * 23 / depth` turned square to the
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

static void beamStripDraw(GfxCoord* coord, SVECTOR* end, s32 cell, s16 width);

#endif /* SRC_SHARED_BEAM_STRIP_H */
