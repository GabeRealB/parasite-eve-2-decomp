#ifndef MAP_DRYFIELD_FULL_H
#define MAP_DRYFIELD_FULL_H

#include "common.h"

/// Record inspected by `func_map_dryfield_full_80179954`. `field_0` is matched
/// against 0x1A - the Dryfield room the junk-yard scripts also key on - and
/// `field_5` gates the hook on 0.
typedef struct MapDryfieldFullRec {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[3];
    /* 0x5 */ u8   field_5;
} MapDryfieldFullRec;

/// Record `func_map_dryfield_full_80179954` writes into: `field_3` receives the
/// junk-yard GameFlag nibble (0x9F) plus one.
typedef struct MapDryfieldFullOut {
    /* 0x0 */ byte pad_0[3];
    /* 0x3 */ s8   field_3;
} MapDryfieldFullOut;

s32 func_map_dryfield_full_80179954(MapDryfieldFullRec* arg0, MapDryfieldFullOut* arg1);

#endif // MAP_DRYFIELD_FULL_H
