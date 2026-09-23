#ifndef ROOMS_NEO_ARK_ALTAR_H
#define ROOMS_NEO_ARK_ALTAR_H

#include "common.h"

/// One 12-byte altar floor tile: `x` / `z` are the tile's low corner in world
/// units, `w` / `d` its size along X and Z, and `id` the number the tile
/// carries. A table scanned by `id` ends at an entry whose `id` is -1.
typedef struct NeoArkAltarTile {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
    /* 0x4 */ s16 w;
    /* 0x6 */ s16 d;
    /* 0x8 */ s16 id;
    /* 0xA */ u16 pad;
} NeoArkAltarTile;
STATIC_ASSERT_SIZEOF(NeoArkAltarTile, 0xC);

/// Per-task work block of the altar task, allocated as 0x10 bytes by
/// `func_neo_ark_altar_8017ED60` and parked in `Task::work` (so the slot holds
/// this overlay's own block, not a `TaskIdMap`). `field_8` is the tile the
/// player currently stands on (`func_neo_ark_altar_8017EC34` of the player
/// coordinate), `field_6` the value recorded on the previous frame, `field_C`
/// the tile whose wall is currently raised and `field_A` that wall's height;
/// `field_E` carries the new tile to `func_neo_ark_altar_8017E260`.
typedef struct NeoArkAltarWork {
    /* 0x0 */ u8  pad_0[0x6];
    /* 0x6 */ s16 field_6;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s16 field_A;
    /* 0xC */ s16 field_C;
    /* 0xE */ s16 field_E;
} NeoArkAltarWork;
STATIC_ASSERT_SIZEOF(NeoArkAltarWork, 0x10);

#endif // ROOMS_NEO_ARK_ALTAR_H
