/* Additive textured beam drawn between two points, for M4A1 Hammer and the gallery. A carrier may define
 * BEAM_STRIP_INCREMENT_DEPTH before including draw.inc.c. */
#ifndef EFFECTS_BEAM_STRIP_BEAM_STRIP_H
#define EFFECTS_BEAM_STRIP_BEAM_STRIP_H

#include <engine/coord.h>

static void _beamStripDraw(const GfxCoord* startCoord, const SVECTOR* worldEnd, s32 textureFrame, s16 widthScale);

#endif /* EFFECTS_BEAM_STRIP_BEAM_STRIP_H */
