/// Static per-carrier interface for the included beam-strip drawer.
///
/// Before including `beam_strip_draw.inc.c`, a carrier may supply the
/// preprocessor boolean `BEAM_STRIP_INCREMENT_DEPTH` (0 disabled, nonzero
/// enabled). It defaults to 1 for M4A1 Hammer; the gallery supplies 0. Enabling
/// it adds one to the start's SZ3 / 4 after each accepted endpoint projection,
/// so a drawn strip uses +2 for both width division and sorting. The fragment
/// undefines the binding after the drawer definition.
#ifndef SRC_SHARED_BEAM_STRIP_H
#define SRC_SHARED_BEAM_STRIP_H

#include "main/coord.h"

static void _beamStripDraw(const GfxCoord* startCoord, const SVECTOR* worldEnd, s32 textureFrame, s16 widthScale);

#endif /* SRC_SHARED_BEAM_STRIP_H */
