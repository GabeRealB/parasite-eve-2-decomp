/* Textured jet drawer shared by Hypervelocity and Pyrokinesis. Its carrier defines, before including draw.inc.c, which
 * undefines them again:
 *   JET_CONE_CLUT                      the palette, a getClut word
 *   JET_CONE_BASE_TAIL_RIM_RADIUS      the rim's radius in the base tail mode, in local units
 *   JET_CONE_EXTENDED_TAIL_RIM_RADIUS  the rim's radius in the extended tail mode
 *   JET_CONE_TEXTURE_FRAME_OFFSETS     an s16 array, one per segment, offsetting each segment's animation frame */
#ifndef EFFECTS_JET_CONE_JET_CONE_H
#define EFFECTS_JET_CONE_JET_CONE_H

#include <core/pe2.h>
#include <gameplay/effects.h>

static void _jetConeDraw(const GfxCoord* coord, s16 ageFrames, s16 baseLength, s32 extendedTail);

#endif
