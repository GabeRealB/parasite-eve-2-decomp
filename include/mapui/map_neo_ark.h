#ifndef MAP_NEO_ARK_H
#define MAP_NEO_ARK_H

#include "common.h"

/// Record inspected by `func_map_neo_ark_80179B14`, one per Neo Ark map room.
/// `field_0` is the room the marker stands for and `field_5` gates the hook on
/// 0, exactly as in `map_dryfield_full`.
typedef struct MapNeoArkRec {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[3];
    /* 0x5 */ u8   field_5;
} MapNeoArkRec;

/// Record `func_map_neo_ark_80179B14` writes into: `field_3` receives the
/// marker's state, derived from the room's GameFlag nibble.
typedef struct MapNeoArkOut {
    /* 0x0 */ byte pad_0[3];
    /* 0x3 */ s8   field_3;
} MapNeoArkOut;

s32 func_map_neo_ark_80179B14(MapNeoArkRec* arg0, MapNeoArkOut* arg1);

#endif // MAP_NEO_ARK_H
