#ifndef ROOMS_NEO_ARK_SHRINE_H
#define ROOMS_NEO_ARK_SHRINE_H

#include "common.h"

/// A pair of 16-bit coordinates used by the shrine's sliding-tile puzzle: the
/// screen position of a tile's quad, or the texture origin it samples from.
/// The puzzle tables index this by tile number or by board position.
typedef struct {
    u16 x;
    u16 y;
} NeoArkShrineSlot;

#endif // ROOMS_NEO_ARK_SHRINE_H
