#ifndef MAP_SHELTER_H
#define MAP_SHELTER_H

#include "common.h"

/// Record inspected by `func_map_shelter_80179A04`, one per Shelter map room.
/// `field_0` is the room the marker stands for and `field_5` gates the hook on
/// 0, exactly as in `map_neo_ark`.
typedef struct MapShelterRec {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[3];
    /* 0x5 */ u8   field_5;
} MapShelterRec;

/// Record `func_map_shelter_80179A04` writes into: `field_3` receives the
/// marker's state, derived from the room's GameFlag nibble.
typedef struct MapShelterOut {
    /* 0x0 */ byte pad_0[3];
    /* 0x3 */ s8   field_3;
} MapShelterOut;

s32 func_map_shelter_80179A04(MapShelterRec* arg0, MapShelterOut* arg1);

#endif // MAP_SHELTER_H
