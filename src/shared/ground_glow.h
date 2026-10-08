/* Included floor-glow drawer shared by Energy Ball and Hypervelocity.
 * Bind GROUND_GLOW_R, GROUND_GLOW_G and GROUND_GLOW_B to GPU colour bytes and
 * GROUND_GLOW_CLUT to a packed palette address before the fragment inclusion.
 * The fragment consumes and undefines those four bindings. */
#ifndef SRC_SHARED_GROUND_GLOW_H
#define SRC_SHARED_GROUND_GLOW_H

#include "common.h"
#include "main/coord.h"

static void _groundGlowDraw(const GfxCoord* ground, s32 halfExtent);

#endif /* SRC_SHARED_GROUND_GLOW_H */
