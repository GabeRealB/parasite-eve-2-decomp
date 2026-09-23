#ifndef ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
#define ROOMS_DRYFIELD_NIGHT_WATER_TANK_H

#include "common.h"

/// Coordinate triple in the room's layout lists, 8 bytes apart; the fourth
/// halfword is never read.
typedef struct DryfieldNightWaterTankVec {
    s16 x;
    s16 y;
    s16 z;
    s16 pad;
} DryfieldNightWaterTankVec;

/// 12-byte record copied whole, byte-aligned, alongside each `field_4` entry.
typedef struct DryfieldNightWaterTankBlob {
    s8 b[12];
} DryfieldNightWaterTankBlob;

/// Pointers to the room's layout lists. `D_dryfield_night_water_tank_8017E08C`
/// is the template and `D_dryfield_night_water_tank_8017F4B0` the working copy
/// that `func_dryfield_night_water_tank_8017D9DC` refills from it: two
/// `field_4` entries with their `field_C` records, and six `field_8` entries
/// which it then shifts. `field_0` is not read there, and nothing pins where
/// the table ends.
typedef struct DryfieldNightWaterTankLayout {
    s32                         field_0;
    DryfieldNightWaterTankVec*  field_4;
    DryfieldNightWaterTankVec*  field_8;
    DryfieldNightWaterTankBlob* field_C;
} DryfieldNightWaterTankLayout;
extern DryfieldNightWaterTankLayout D_dryfield_night_water_tank_8017E08C;
extern DryfieldNightWaterTankLayout D_dryfield_night_water_tank_8017F4B0;

#endif // ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
