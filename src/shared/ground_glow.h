/* Ground glow: a flat quad lit on the floor under an effect - the unit quad
 * `D_80111E38` scaled to `size`, laid in the ground plane through
 * `gGfxViewCoord.workm` and moved onto the traced ground point `ground`, then
 * projected (one RTPS, one RTPT, a single flag check) as a semi-transparent
 * POLY_FT4 of the two-frame strip at tpage 0x28, rows 0x38-0x57, flickering
 * with `gDisplayState.animFrame`. Hypervelocity's scorch and the Energy
 * Ball's glow; the fireball library's `_fireballDrawGroundGlow` is the same
 * effect built differently (the flag checked after each projection).
 *
 * The including unit sets GROUND_GLOW_R/_G/_B (tint) and GROUND_GLOW_CLUT.
 */
#ifndef SRC_SHARED_GROUND_GLOW_H
#define SRC_SHARED_GROUND_GLOW_H

#include "common.h"
#include "main/coord.h"

static void groundGlowDraw(GfxCoord* ground, s32 size);

#endif /* SRC_SHARED_GROUND_GLOW_H */
