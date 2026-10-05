/* The animated flame billboard shared by Pyrokinesis and Combustion.
 *
 * Include this interface in the carrier's prologue and the drawer fragment at
 * its function's position. Each carrier keeps its own static instance.
 */

#ifndef SRC_SHARED_PYRO_FLAME_H
#define SRC_SHARED_PYRO_FLAME_H

#include "types.h"

#include "main/coord.h"

/// Number of 32-texel cells in the flame's horizontal animation strip.
enum { PYRO_FLAME_FRAME_COUNT = 8 };

/// Texture coordinates are in texels; angles use 4096 units per turn and Q12 trigonometry.
enum {
    PYRO_FLAME_CELL_WIDTH         = 32,
    PYRO_FLAME_UV_SPAN            = PYRO_FLAME_CELL_WIDTH - 1,
    PYRO_FLAME_TOP_V              = 24,
    PYRO_FLAME_BOTTOM_V           = PYRO_FLAME_TOP_V + PYRO_FLAME_UV_SPAN,
    PYRO_FLAME_DEPTH_BIAS         = 1,
    PYRO_FLAME_FULL_TURN          = 4096,
    PYRO_FLAME_QUARTER_TURN       = PYRO_FLAME_FULL_TURN / 4,
    PYRO_FLAME_TRIG_FRACTION_BITS = 12,
};

static void _pyroFlameDrawSprite(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 angle);

#endif /* SRC_SHARED_PYRO_FLAME_H */
