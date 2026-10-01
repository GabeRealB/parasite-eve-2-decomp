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
 *   SPRITE_QUAD_CELL_H      optional, with CELLS_PER_ROW: a grid of cells this
 *                           tall, the rows offset from V0/V1
 *   SPRITE_QUAD_UV_TABLE    optional: a GpEffUv8 table giving each frame's
 *                           square cell (SPRITE_QUAD_CLUT may then also be an
 *                           expression of `frame`)
 *   SPRITE_QUAD_CELL_MASK   optional: the frame's low bits pick the cell
 *   SPRITE_QUAD_U_BASE      optional: texel column of the first cell
 *   SPRITE_QUAD_MIN_OTZ     optional: draw only at this depth or beyond
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

/* Where the position comes from: a coordinate's world matrix by default, or a
 * bare translation (`long[3]`, SPRITE_QUAD_POS_T long). Set before including. */
#ifndef SPRITE_QUAD_POS_T
#define SPRITE_QUAD_POS_T     GfxCoord
#define SPRITE_QUAD_POS(p, i) ((p)->workm.t[i])
#endif
#ifndef SPRITE_QUAD_SIZE_T
#define SPRITE_QUAD_SIZE_T s16
#endif

#ifndef SPRITE_QUAD_FRAME_T
#define SPRITE_QUAD_FRAME_T u16
#endif

static void spriteQuadDraw(SPRITE_QUAD_POS_T* pos, SPRITE_QUAD_FRAME_T frame, SPRITE_QUAD_SIZE_T size, s16 angle);

#endif /* SRC_SHARED_SPRITE_QUAD_H */
