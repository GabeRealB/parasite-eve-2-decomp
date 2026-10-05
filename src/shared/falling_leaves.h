/* Included falling-leaf effects for the Acropolis and Neo Ark rooms.
 * Include this interface in the carrier's prologue, the inline task fragment
 * before its exported room wrapper, and the selected drawer at its original
 * function position. The Acropolis and Neo Ark drawers use different texture
 * cells and projection rejection rules. All shared function instances are
 * private; gameplay's effect table reaches each room's wrapper.
 */

#ifndef SRC_SHARED_FALLING_LEAVES_H
#define SRC_SHARED_FALLING_LEAVES_H

#include "types.h"

#include "main/coord.h"

/// Selects an opaque, raw-textured leaf instead of brightness modulation.
enum { LEAF_BRIGHTNESS_RAW_TEXTURE = 0 };

static void _leafDraw(const GfxCoord* coord, s32 halfSize, s16 brightness);

#endif /* SRC_SHARED_FALLING_LEAVES_H */
