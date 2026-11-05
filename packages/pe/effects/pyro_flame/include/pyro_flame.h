/* The animated flame billboard shared by Pyrokinesis and Combustion.
 *
 * Include this interface in the carrier's prologue and the drawer fragment at
 * its function's position. Each carrier keeps its own static instance.
 */

#ifndef PE_EFFECTS_PYRO_FLAME_PYRO_FLAME_H
#define PE_EFFECTS_PYRO_FLAME_PYRO_FLAME_H

#include <core/pe2.h>

#include <engine/coord.h>

/// Number of 32-texel cells in the flame's horizontal animation strip.
enum { PYRO_FLAME_FRAME_COUNT = 8 };

/// The flame's texture cells, in texels, and its angles, 4096 to a turn.
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

#endif /* PE_EFFECTS_PYRO_FLAME_PYRO_FLAME_H */
