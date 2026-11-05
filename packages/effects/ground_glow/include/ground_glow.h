/* Flickering floor glow shared by Energy Ball and Hypervelocity. Its carrier defines, before including draw.inc.c,
 * which undefines them again, GROUND_GLOW_R, GROUND_GLOW_G and GROUND_GLOW_B, the tint (0x80 leaves the texture as
 * it is), and GROUND_GLOW_CLUT, the palette as a getClut word. */
#ifndef EFFECTS_GROUND_GLOW_GROUND_GLOW_H
#define EFFECTS_GROUND_GLOW_GROUND_GLOW_H

#include <core/pe2.h>
#include <engine/coord.h>

static void _groundGlowDraw(const GfxCoord* ground, s32 halfExtent);

#endif /* EFFECTS_GROUND_GLOW_GROUND_GLOW_H */
