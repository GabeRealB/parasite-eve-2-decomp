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
 * second instance (the first is spriteQuadDraw, declared here).
 */

#ifndef SRC_SHARED_SPRITE_QUAD_H
#define SRC_SHARED_SPRITE_QUAD_H

#include "common.h"

#include "gameplay/effects.h"

#ifndef SPRITE_QUAD_POSITION_SOURCE_TYPE
/// Read-only source type for an included sprite-quad drawer's translation.
///
/// The signature adds a pointer to this type. The default `const GfxCoord`
/// reads `workm.t[0..2]`; `const long` reads three consecutive signed 32-bit
/// translation words, as supplied by Hammer's charge sprite and Javelin.
/// A custom binding must also define `SPRITE_QUAD_POS` to read components
/// 0, 1 and 2 from a pointer to that type.
///
/// Supply components in the input space of `GsWSMATRIX`, in game-coordinate
/// units. Only their low 16 bits reach the signed `SVECTOR` used for projection.
/// The source is borrowed for the call, never modified or retained; coordinate
/// caches must already contain the intended translation because the drawer
/// performs no composition.
///
/// Bind before this header for the first instance's declaration. The binding
/// persists across `sprite_quad_draw.inc.c` inclusions; changing an instance
/// requires undefining and redefining both the type and `SPRITE_QUAD_POS`,
/// with a consistent forward declaration. The flicker drawer has a fixed type.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const GfxCoord
#define SPRITE_QUAD_POS(p, i)            ((p)->workm.t[i])
#endif
#ifndef SPRITE_QUAD_SIZE_T
#define SPRITE_QUAD_SIZE_T s16
#endif

#ifndef SPRITE_QUAD_FRAME_T
/// Integer type of the texture-frame selector for an included sprite-quad drawer.
///
/// Define as `s16` or `u16` before including this header to select the first
/// instance's signature. The unsigned default serves the flare,
/// shelter_b6_nursery and actor_510900 instances. Signed carriers override it
/// for their effect counters.
/// The type narrows the argument to 16 bits and determines its signed or unsigned
/// interpretation in texture-coordinate arithmetic.
///
/// `frame` counts texture frames. A strip uses it as a cell number, optionally
/// masked or reduced modulo the row width; a grid also derives the row from it.
/// Computed UV coordinates narrow to the GPU's byte fields. With
/// `SPRITE_QUAD_UV_TABLE`, the caller must instead supply a nonnegative index
/// within that table and any frame-indexed `SPRITE_QUAD_CLUT` table.
///
/// The binding persists across `sprite_quad_draw.inc.c` inclusions. Undefine and
/// redefine it when changing an instance's signature, and keep that instance's
/// forward declaration consistent. Hammer uses `u16` for its charge sprite and
/// restores `s16` for its six-cell strip. The flicker drawer has a fixed signature.
#define SPRITE_QUAD_FRAME_T u16
#endif

static void spriteQuadDraw(SPRITE_QUAD_POSITION_SOURCE_TYPE* pos, SPRITE_QUAD_FRAME_T frame, SPRITE_QUAD_SIZE_T size, s16 angle);

/* The flicker form (sprite_quad_draw_flicker.inc.c) alternates two looks of
 * the flame strip; these are its two cells. */
#define SPRITE_QUAD_CORE_CELL(p) \
    (p)->tpage = 0x29;           \
    (p)->clut  = 0x428B;         \
    setUV4(p, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF)
#define SPRITE_QUAD_RIM_CELL(p) \
    (p)->tpage = 0x29;          \
    (p)->clut  = 0x428C;        \
    setUV4(p, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF)

static void spriteQuadDrawFlicker(GfxCoord* coord, s16 frame, s16 size, s16 angle);

#endif /* SRC_SHARED_SPRITE_QUAD_H */
