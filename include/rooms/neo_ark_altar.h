#ifndef ROOMS_NEO_ARK_ALTAR_H
#define ROOMS_NEO_ARK_ALTAR_H

#include "common.h"

/// One 12-byte entry of `D_neo_ark_altar_8017F014`, the altar's floor-tile
/// table: `x` / `z` are the tile's low corner in world units, `w` / `d` its
/// size along X and Z, and `id` the 1-based number the tile carries in the
/// table (`0xFFFF` on the terminator that ends it).
typedef struct NeoArkAltarTile {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 z;
    /* 0x4 */ u16 w;
    /* 0x6 */ u16 d;
    /* 0x8 */ u16 id;
    /* 0xA */ u16 pad;
} NeoArkAltarTile;
STATIC_ASSERT_SIZEOF(NeoArkAltarTile, 0xC);

#endif // ROOMS_NEO_ARK_ALTAR_H
