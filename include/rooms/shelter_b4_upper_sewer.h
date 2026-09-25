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

#endif // ROOMS_SHELTER_B4_UPPER_SEWER_H
