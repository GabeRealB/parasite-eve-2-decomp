/* Included floor-glow drawer shared by Energy Ball and Hypervelocity.
 * Bind GROUND_GLOW_R, GROUND_GLOW_G and GROUND_GLOW_B to GPU colour bytes and
 * GROUND_GLOW_CLUT to an integer constant expression encoding a GPU palette
 * selector for POLY_FT4::clut before the fragment inclusion. Use the SDK's
 * getClut(x, y), with X in 16-bit VRAM words (0..1008, aligned to sixteen)
 * and Y in VRAM rows (0..511), to select sixteen resident colours for the
 * 4-bit texture. The selector packs Y in bits 6..14 and X / 16 in bits 0..5;
 * it fits u16.
 * GROUND_GLOW_G supplies green texture modulation: an integer constant in
 * 0..255, with 0x80 preserving the sampled green intensity. It is stored once
 * in POLY_FT4::g0 and has no arguments or captured identifiers.
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
