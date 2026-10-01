/* Jet cone: a sixteen-segment textured cone hanging off an effect coordinate -
 * a rim ring sunk `back` along the coordinate's -Z, joined to a small hub ring
 * at its origin, each segment a semi-transparent POLY_FT4 from the six-frame
 * flame strip (tpage 0x2A, v 0x60-0x87) sorted at its own depth. `shortCone`
 * picks the short, wide cone (hub 0x80, `back = length * 2 + age * 256`) or
 * the long, narrow one (hub 0x40, `back = length + age * 16`), so a caller
 * draws both for a two-layer jet. Used by Hypervelocity's trail and
 * Pyrokinesis's flame.
 *
 * The including unit sets, before including the fragment:
 *   JET_CONE_CLUT          the strip's CLUT
 *   JET_CONE_RIM_SHORT     rim radius of the short cone
 *   JET_CONE_RIM_LONG      rim radius of the long cone
 *   JET_CONE_FRAME_JITTER  s16[16]: per-segment frame offsets, re-rolled by the caller
 */
#ifndef SRC_SHARED_JET_CONE_H
#define SRC_SHARED_JET_CONE_H

#include "common.h"
#include "gameplay/effects.h"

static void jetConeDraw(GfxCoord* coord, s16 age, s16 length, s32 shortCone);

#endif
