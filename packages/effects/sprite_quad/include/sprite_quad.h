/* Sprite quad: the copy each including unit makes of gameplay's `effectDrawSpinningBillboard`, with its texture fixed.
 * It projects a world position and, unless that lands behind the camera, draws a camera-facing textured quad there:
 * `frame` picks a texture cell, `angle` spins the quad, and `size` times SPRITE_QUAD_SCALE over the projected depth
 * is its half-diagonal in pixels.
 *
 * The including unit defines the texture before each inclusion of draw.inc.c, which undefines it all again, so a
 * unit drawing two textures binds and includes twice:
 *   SPRITE_QUAD_FUNC           the function's name (default spriteQuadDraw)
 *   SPRITE_QUAD_CLUT           palette word, which may read `frame` (required)
 *   SPRITE_QUAD_CELL_WIDTH     cell width in texels, also the U stride between cells (required)
 *   SPRITE_QUAD_TOP_V          first cell row's top texel row (required without UV_TABLE)
 *   SPRITE_QUAD_BOTTOM_V       first cell row's bottom texel row, inclusive (required without UV_TABLE)
 *   SPRITE_QUAD_SCALE          positive size multiplier, usually the cell's width less one (required)
 *   SPRITE_QUAD_CELLS_PER_ROW  cells across a row: the column is frame % count
 *   SPRITE_QUAD_CELL_HEIGHT    row stride in texels, with CELLS_PER_ROW: the row is frame / count
 *   SPRITE_QUAD_COLUMN_MASK    low bits of `frame` picking the column, 2^n - 1 (CELLS_PER_ROW wins over it)
 *   SPRITE_QUAD_U_BASE         texel column of the first cell (default 0)
 *   SPRITE_QUAD_UV_TABLE       EffectSpriteTextureFrame table giving each frame's UV origin instead
 *   SPRITE_QUAD_MIN_OTZ        nearest depth drawn, in SZ3 / 4 units
 *   SPRITE_QUAD_INCREMENT_DEPTH  0 drops the one added to the projected depth (default 1)
 *   SPRITE_QUAD_TEXTURE_PAGE   texture page (default getTPage(0, GPU_BLEND_ADD, 640, 0))
 * With none of CELLS_PER_ROW, COLUMN_MASK and UV_TABLE, the column is `frame` itself.
 */

#ifndef EFFECTS_SPRITE_QUAD_SPRITE_QUAD_H
#define EFFECTS_SPRITE_QUAD_SPRITE_QUAD_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include <core/pe2.h>

#include <engine/display_types.h>

#include <gameplay/effects.h>

/// Scratch block for one camera-facing quad: its projected center and the offset to a pair of opposite corners, which a
/// spinning quad computes twice, a quarter turn apart.
typedef struct {
    s32     otz;       // Projected depth (SZ3 / 4), also the divisor sizing the corners
    s32     cornerDx;  // Pixels to the current corner pair along X; an upright quad's half-width
    s32     cornerDy;  // Pixels to the current corner pair along Y; an upright quad's half-height
    SVECTOR worldPos;  // Center in world coordinates
    DVECTOR screenPos; // Projected center in screen pixels
} SpriteQuadScratch;

#ifndef SPRITE_QUAD_POSITION_SOURCE_TYPE
/// What the drawer reads its position from: a `GfxCoord`'s translation by default. Hammer's charge sprite and Javelin
/// bind `const long`, three words, with a matching SPRITE_QUAD_TRANSLATION_COMPONENT. Stays bound across inclusions.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const GfxCoord
/// Reads component 0, 1 or 2 (X, Y, Z) of the position source; only its low 16 bits are projected.
#define SPRITE_QUAD_TRANSLATION_COMPONENT(source, componentIndex) ((source)->workm.t[(componentIndex)])
#endif
#ifndef SPRITE_QUAD_SIZE_T
/// Type of the drawer's `size`: `s16` by default, or `u16`. Stays bound across inclusions.
#define SPRITE_QUAD_SIZE_T s16
#endif

#ifndef SPRITE_QUAD_FRAME_T
/// Type of the drawer's `frame`: `u16` by default, or `s16`. Stays bound across inclusions.
#define SPRITE_QUAD_FRAME_T u16
#endif

/// Projects `pos` and draws one camera-facing cell of the texture: `frame` picks the cell, `angle` spins it (4096 to a
/// turn), and `size` times SPRITE_QUAD_SCALE over the depth is its half-diagonal in pixels.
static void spriteQuadDraw(SPRITE_QUAD_POSITION_SOURCE_TYPE* pos, SPRITE_QUAD_FRAME_T frame, SPRITE_QUAD_SIZE_T size, s16 angle);

/// Sets a flicker quad's texture to the 56-texel flame cell of odd frames, with its additive page and palette; the
/// including unit adds the tint and flags.
#define SPRITE_QUAD_SET_FLICKER_ODD_CELL(quad)                                                                                     \
    {                                                                                                                              \
        enum { SPRITE_QUAD_FLICKER_LEFT_U  = 0x70,                                                                                 \
               SPRITE_QUAD_FLICKER_TOP_V   = 0xC8,                                                                                 \
               SPRITE_QUAD_FLICKER_UV_SPAN = 56 - 1 };                                                                             \
        (quad)->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);                                                                        \
        (quad)->clut  = getClut(176, 266);                                                                                         \
        setUV4((quad), SPRITE_QUAD_FLICKER_LEFT_U, SPRITE_QUAD_FLICKER_TOP_V,                                                      \
               SPRITE_QUAD_FLICKER_LEFT_U + SPRITE_QUAD_FLICKER_UV_SPAN, SPRITE_QUAD_FLICKER_TOP_V,                                \
               SPRITE_QUAD_FLICKER_LEFT_U, SPRITE_QUAD_FLICKER_TOP_V + SPRITE_QUAD_FLICKER_UV_SPAN,                                \
               SPRITE_QUAD_FLICKER_LEFT_U + SPRITE_QUAD_FLICKER_UV_SPAN, SPRITE_QUAD_FLICKER_TOP_V + SPRITE_QUAD_FLICKER_UV_SPAN); \
    }

/// The same for the flame cell of even frames.
#define SPRITE_QUAD_SET_FLICKER_EVEN_CELL(quad)                                                                                    \
    {                                                                                                                              \
        enum { SPRITE_QUAD_FLICKER_LEFT_U  = 0xA8,                                                                                 \
               SPRITE_QUAD_FLICKER_TOP_V   = 0xC8,                                                                                 \
               SPRITE_QUAD_FLICKER_UV_SPAN = 56 - 1 };                                                                             \
        (quad)->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);                                                                        \
        (quad)->clut  = getClut(192, 266);                                                                                         \
        setUV4((quad), SPRITE_QUAD_FLICKER_LEFT_U, SPRITE_QUAD_FLICKER_TOP_V,                                                      \
               SPRITE_QUAD_FLICKER_LEFT_U + SPRITE_QUAD_FLICKER_UV_SPAN, SPRITE_QUAD_FLICKER_TOP_V,                                \
               SPRITE_QUAD_FLICKER_LEFT_U, SPRITE_QUAD_FLICKER_TOP_V + SPRITE_QUAD_FLICKER_UV_SPAN,                                \
               SPRITE_QUAD_FLICKER_LEFT_U + SPRITE_QUAD_FLICKER_UV_SPAN, SPRITE_QUAD_FLICKER_TOP_V + SPRITE_QUAD_FLICKER_UV_SPAN); \
    }

static void _spriteQuadDrawFlicker(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 spinAngle);

#endif /* EFFECTS_SPRITE_QUAD_SPRITE_QUAD_H */
