#ifndef ROOMS_SHELTER_B4_UPPER_SEWER_H
#define ROOMS_SHELTER_B4_UPPER_SEWER_H

#include "common.h"

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z, cut into `count` flat quads. The quads are laid along X
/// when `alongZ` is zero and along Z otherwise. A list of them ends at an entry
/// whose `count` is -1.
typedef struct ShelterB4UpperSewerSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s16 count;
    s16 alongZ;
} ShelterB4UpperSewerSurface;

/// Per-surface values the water drawer keeps in a block taken from the
/// scratchpad stack at `0x1F8003FC` rather than in registers. `dx` and `dz`
/// are the spacing between vertices along X and Z, and `wave` a height added
/// to the far edge of each quad, always zero here. The layout is the one the
/// water drawers of `shelter_b4_water_supply` use; whether the two are one
/// type is open.
typedef struct ShelterB4UpperSewerWaterWork {
    s16 y;
    s16 dx;
    s16 wave;
    s16 dz;
    s16 x;
    s16 z;
} ShelterB4UpperSewerWaterWork;

#endif // ROOMS_SHELTER_B4_UPPER_SEWER_H
