/* Included floor-glow drawer shared by Energy Ball and Hypervelocity.
 * Bind GROUND_GLOW_R, GROUND_GLOW_G and GROUND_GLOW_B to GPU colour bytes and
 * GROUND_GLOW_CLUT to a packed palette address before the fragment inclusion.
 * GROUND_GLOW_B supplies blue texture modulation: an integer constant in
 * 0..255, with 0x80 preserving the sampled blue intensity. Energy Ball and
 * Hypervelocity supply their own values immediately before the inclusion.
 * The fragment consumes and undefines those four bindings. */
#ifndef SRC_SHARED_GROUND_GLOW_H
#define SRC_SHARED_GROUND_GLOW_H

#include "common.h"
#include "main/coord.h"

static void _groundGlowDraw(const GfxCoord* ground, s32 halfExtent);

#endif /* SRC_SHARED_GROUND_GLOW_H */
