/* Included textured-jet drawer shared by Hypervelocity and Pyrokinesis.
 * Pyrokinesis and Hypervelocity bind JET_CONE_CLUT before the drawer inclusion
 * to an integer constant selecting a sixteen-colour palette via getClut.
 * Its X coordinate counts 16-bit VRAM words and must be a multiple of 16 in
 * 0..1008; Y counts VRAM rows in 0..511. The palette must be loaded before
 * drawing. The drawer inclusion undefines this binding after using it.
 * Bind JET_CONE_RIM_SHORT and
 * JET_CONE_RIM_LONG to local-unit rim radii for the extended and base tails;
 * and JET_CONE_FRAME_JITTER to a readable s16[16] frame-offset array.
 * Keep the bindings through the fragment inclusion. */
#ifndef SRC_SHARED_JET_CONE_H
#define SRC_SHARED_JET_CONE_H

#include "common.h"
#include "gameplay/effects.h"

static void _jetConeDraw(const GfxCoord* coord, s16 ageFrames, s16 baseLength, s32 extendedTail);

#endif
