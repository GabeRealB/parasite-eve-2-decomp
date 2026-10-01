/* Sprite quad: the overlays' copy of gameplay's `Gp_DrawFxQuad` with the
 * texture fixed per overlay. A world position is projected through
 * `GsWSMATRIX` and, unless it lands behind the camera, a camera-facing
 * textured POLY_FT4 is drawn there: `frame` picks a cell along the texture
 * row, `angle` spins the quad and `size` sets its half-diagonal, divided by
 * depth so the sprite shrinks into the distance.
 *
 * The including unit sets the texture before including the fragment:
 *   SPRITE_QUAD_CLUT        CLUT word
 *   SPRITE_QUAD_CELL_W      cell width in texels (cells sit side by side)
 *   SPRITE_QUAD_V0/_V1      top and bottom texel rows
 *   SPRITE_QUAD_SCALE       size multiplier (the cell's half-width in texels)
 *   SPRITE_QUAD_CELLS_PER_ROW  optional: frames wrap after this many cells
 *   SPRITE_QUAD_OTZ_BIAS    1 (default) sorts the sprite one slot behind its
 *                           point, 0 at the point itself
 * and SPRITE_QUAD_TPAGE when it is not 0x2A. The fragment clears these, so a
 * unit drawing two textures includes it twice; SPRITE_QUAD_FUNC names the
 * second instance (the first is spriteQuadDraw, declared here). A unit whose callers pass the
 * frame signed defines SPRITE_QUAD_FRAME_T as s16 before including this header.
 */

#ifndef SRC_SHARED_SPRITE_QUAD_H
#define SRC_SHARED_SPRITE_QUAD_H

#include "common.h"

#include "gameplay/effects.h"

#ifndef SPRITE_QUAD_FRAME_T
#define SPRITE_QUAD_FRAME_T u16
#endif

static void spriteQuadDraw(GfxCoord* coord, SPRITE_QUAD_FRAME_T frame, s16 size, s16 angle);

#endif /* SRC_SHARED_SPRITE_QUAD_H */
