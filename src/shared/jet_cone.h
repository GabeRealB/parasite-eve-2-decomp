/* Included textured-jet drawer shared by Hypervelocity and Pyrokinesis.
 * Pyrokinesis and Hypervelocity bind JET_CONE_CLUT before the drawer inclusion
 * to an integer constant selecting a sixteen-colour palette via getClut.
 * Its X coordinate counts 16-bit VRAM words and must be a multiple of 16 in
 * 0..1008; Y counts VRAM rows in 0..511. The palette must be loaded before
 * drawing. The drawer inclusion undefines this binding after using it.
 * Bind JET_CONE_RIM_SHORT to the extended tail's local-unit rim radius;
 * keep that binding through the fragment inclusion.
 * Bind JET_CONE_BASE_TAIL_RIM_RADIUS to the base tail's XY rim radius in
 * whole local coordinate units, as a signed integer constant in 0..32767.
 * Pyrokinesis and Hypervelocity supply independent values before including
 * the drawer; it checks the range and undefines the base-tail binding.
 * JET_CONE_TEXTURE_FRAME_OFFSETS selects a readable s16 array with at least
 * EFFECT_BAND_SEGMENT_COUNT elements in segment order. Bind a stable array
 * identifier before including jet_cone_draw.inc.c; that fragment undefines
 * the binding afterwards. Each accepted segment reads its offset once per
 * draw, adds ageFrames after promotion to s32 and selects a texture cell
 * modulo six. The sum must be nonnegative for valid texture coordinates.
 * Both carriers store offsets 0..255 at cast/round startup and borrow their
 * overlay's current table for both tail modes; the drawer retains no pointer. */
#ifndef SRC_SHARED_JET_CONE_H
#define SRC_SHARED_JET_CONE_H

#include "common.h"
#include "gameplay/effects.h"

static void _jetConeDraw(const GfxCoord* coord, s16 ageFrames, s16 baseLength, s32 extendedTail);

#endif
