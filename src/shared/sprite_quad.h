/* Sprite quad: the overlays' copy of gameplay's `effectDrawSpinningBillboard` with the
 * texture fixed per overlay. A world position is projected through
 * `GsWSMATRIX` and, unless it lands behind the camera, a camera-facing
 * textured POLY_FT4 is drawn there: `frame` selects a texture cell,
 * `angle` spins the quad and `size` combines with the carrier's scale
 * and projected depth to set its screen-space half-diagonal.
 *
 * The including unit sets the texture before including the fragment:
 *   SPRITE_QUAD_CLUT        packed CLUT word, or an expression of `frame`
 *   SPRITE_QUAD_CELL_WIDTH  cell width in texels, including both endpoints
 *   SPRITE_QUAD_TOP_V       inclusive top texel row of the first cell row
 *   SPRITE_QUAD_V1          inclusive bottom texel row of the first cell row
 *   SPRITE_QUAD_SCALE       signed perspective-sizing multiplier
 *   SPRITE_QUAD_CELLS_PER_ROW  optional: number of cell columns, not texels
 *   SPRITE_QUAD_CELL_H      optional, with CELLS_PER_ROW: a grid of cells this
 *                           tall, added to TOP_V/V1 for each row
 *   SPRITE_QUAD_UV_TABLE    optional: an EffectSpriteTextureFrame table supplying
 *                           UV origins; its palette fields are ignored
 *                           (SPRITE_QUAD_CLUT may be an expression of `frame`)
 *   SPRITE_QUAD_CELL_MASK   optional: the frame's low bits pick the cell
 *   SPRITE_QUAD_U_BASE      optional: integer first-cell texel column (0..255)
 *   SPRITE_QUAD_MIN_OTZ     optional: draw only at this depth or beyond
 *   SPRITE_QUAD_OTZ_BIAS    1 (default) sorts the sprite one slot behind its
 *                           point, 0 at the point itself
 *   SPRITE_QUAD_TEXTURE_PAGE optional: packed 16-bit GPU texture-page word;
 *                           the default is getTPage(0, GPU_BLEND_ADD, 640, 0)
 * The texture-page binding's contract is beside the fragment's default.
 *
 * SPRITE_QUAD_CLUT is required and has no default. It is the 16-bit palette
 * selector stored in POLY_FT4::clut. A constant is a packed getClut word:
 * scanline Y in bits 6..15 and the palette X, in 16-word steps, in bits 0..5.
 * An expression may read `frame` and must not change it; actor 510900 indexes
 * its per-frame palette table that way. The drawer assigns the value once for
 * each emitted quad. The fragment undefines the binding, so each texture
 * supplies its own.
 *
 * SPRITE_QUAD_FUNC names the function that inclusion defines. Leave it unset
 * and the instance is spriteQuadDraw, declared below. Bind an identifier that
 * matches a static declaration in the carrier for another instance: antibody's
 * mote and Hammer's charge flare do. The fragment undefines the name, so a
 * later inclusion in the same file is spriteQuadDraw unless bound again.
 * The fragment clears the other bindings too, so a unit drawing two textures
 * includes it twice.
 *
 * SPRITE_QUAD_SCALE must be a positive signed integer constant, bound before
 * each sprite_quad_draw.inc.c inclusion. A C enum constant is valid. It
 * multiplies the 16-bit size argument after promotion to signed 32-bit
 * arithmetic: size * scale / depth is the screen-space half-diagonal in
 * pixels before rotation. Integer division truncates toward zero before the
 * Q12 sine/cosine multiplication and right shift. The depth is SZ3 / 4,
 * with SPRITE_QUAD_OTZ_BIAS already applied, and must be nonzero when sizing.
 *
 * Current carriers choose the inclusive horizontal UV span, cell width - 1:
 * 23, 31, 39, 47 or 55 for cells 24, 32, 40, 48 or 56 texels wide. The atlas
 * instance uses EFFECT_SPRITE_ATLAS_UV_SPAN. This binding sets geometry;
 * texture addressing still uses SPRITE_QUAD_CELL_WIDTH independently.
 * The fragment undefines the scale after each instance, including both
 * instances in antibody and m4a1_hammer; each inclusion needs its own binding.
 * The flicker drawer uses the same sizing formula with a fixed signed size
 * argument and depth bias of one; its three carriers bind 55 for the flame
 * cells spanning 56 texels, and its fragment also undefines the scale.
 *
 * SPRITE_QUAD_CELL_WIDTH must be a signed integer constant from 1 to 256,
 * bound before each sprite_quad_draw.inc.c inclusion. A C enum constant is
 * valid, as in actor_510900's shared atlas binding. Arithmetic UV modes use
 * this width as the horizontal origin stride and place the inclusive right
 * edge width - 1 texels past the origin. With SPRITE_QUAD_UV_TABLE it sets
 * both dimensions of each square cell; the table supplies the origins.
 * It does not set the frame count or the screen-space size. Texture
 * coordinates narrow to GPU bytes, preserving wraparound. The fragment
 * undefines the width after each instance, including both instances in
 * antibody and m4a1_hammer; each inclusion needs its own binding.
 *
 * SPRITE_QUAD_CELLS_PER_ROW must be a positive signed integer preprocessor
 * constant, bound before each sprite_quad_draw.inc.c inclusion. In arithmetic
 * UV mode it selects the column with frame % count, taking precedence over
 * SPRITE_QUAD_CELL_MASK. Without SPRITE_QUAD_CELL_H, only the column repeats
 * and TOP_V/V1 stay fixed. With CELL_H, the count is required and frame / count
 * selects the row; the count does not limit the number of rows or frames.
 * Signed frames retain signed division/remainder behavior, and the resulting
 * UV coordinates narrow to GPU bytes. SPRITE_QUAD_UV_TABLE bypasses this
 * arithmetic. The fragment undefines the count after each instance, so the
 * next texture needs its own binding. Current bindings are six columns for
 * both antibody strips, the gallery, training room and Hammer, and five for
 * sterilization's grid.
 *
 * SPRITE_QUAD_TOP_V must be a signed integer constant expression, bound before
 * each arithmetic-UV sprite_quad_draw.inc.c inclusion. It counts texels from
 * the selected texture-page origin to the first cell row's inclusive top edge,
 * assigned to texture vertices 0 and 1; rotation may move that edge anywhere
 * on screen. An enum constant is valid. The binding has no parameters or
 * captured locals, and is evaluated twice for each emitted arithmetic-mode quad.
 *
 * Strips keep this V coordinate fixed for every frame. With SPRITE_QUAD_CELL_H,
 * the drawer first adds (s16)(frame / SPRITE_QUAD_CELLS_PER_ROW) * CELL_H using
 * signed arithmetic, then setUV4 narrows both top coordinates modulo 256 into
 * GPU bytes. Sterilization retains -0x80 for its first row at V=0x80; the next
 * row starts at V=0xB0 after adding the 48-texel stride. The origin does not
 * set the cell height, row count, frame count or screen-space extent.
 *
 * SPRITE_QUAD_UV_TABLE bypasses this binding entirely and need not define it.
 * The fragment undefines TOP_V after every inclusion, including both instances
 * in antibody and m4a1_hammer, so each arithmetic instance supplies its own.
 * The flicker drawer uses its separate core/rim cell bindings.
 *
 * SPRITE_QUAD_U_BASE shifts computed cell columns within the selected texture
 * page; leaving it undefined starts at column zero. It is ignored when
 * SPRITE_QUAD_UV_TABLE supplies the origins. For a strip, the right edge uses
 * a signed-byte first-cell endpoint; setUV4 stores both edges modulo 256.
 * Pyrokinesis and Hypervelocity bind 0x70 for their two-cell flame strips.
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
/// Integer type of the perspective-size argument for an included sprite-quad drawer.
///
/// Bind to `s16` (the default) or `u16` before this header for the first
/// instance's declaration. The call narrows `size` to 16 bits, interpreted as
/// -32768..32767 with `s16` or 0..65535 with `u16`.
/// Both types promote to signed 32-bit arithmetic in the sizing calculation.
///
/// `size` is an integer sizing numerator: `size * SPRITE_QUAD_SCALE / depth`
/// gives the signed screen-space half-diagonal in pixels before rotation and
/// rounding. `depth` is the projected SZ3 divided by four, with the instance's
/// optional ordering-depth bias already applied. The argument is neither a
/// pixel count nor a fixed-point fraction; its pixel extent depends on the
/// carrier's scale and the projected depth.
///
/// The binding persists across `sprite_quad_draw.inc.c` inclusions. Undefine
/// and redefine it when changing an instance's signature, and keep that
/// instance's forward declaration consistent. Hammer uses `u16` for its charge
/// flare and restores `s16` for its six-cell strip; all other current instances
/// use the signed default. The flicker drawer has a fixed `s16` size argument.
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

/// Projects `pos` and draws one camera-facing cell of the carrier's sprite texture.
///
/// `frame` selects the cell under the carrier's UV bindings. `size` is the
/// perspective-size numerator: `size * SPRITE_QUAD_SCALE / depth` is the
/// screen-space half-diagonal in pixels before rotation. `angle` is the spin
/// in 4096 units per turn. `SPRITE_QUAD_CLUT` and the texture-page binding
/// select the palette and page.
///
/// `pos` supplies a world translation in `GsWSMATRIX` input space; only the
/// low 16 bits of each component are projected, and the source is borrowed for
/// the call. Nothing is drawn when the point is behind the camera, or when a
/// bound minimum depth rejects it. Depth must be nonzero when a quad is sized.
/// The primitive is semitransparent and unshaded, ordered one slot behind the
/// point unless `SPRITE_QUAD_OTZ_BIAS` is 0.
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

static void _spriteQuadDrawFlicker(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 spinAngle);

#endif /* SRC_SHARED_SPRITE_QUAD_H */
