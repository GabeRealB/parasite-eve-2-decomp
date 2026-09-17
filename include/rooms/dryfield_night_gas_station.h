#ifndef ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
#define ROOMS_DRYFIELD_NIGHT_GAS_STATION_H

#include "common.h"

/// One of the per-view objects the room's sprite-table record points at. Its
/// three flags sit eight bytes apart -- one `GpSprtCmd` record each -- and
/// `func_dryfield_night_gas_station_80180DC8` writes all three together, to 1
/// to light the lamp and to 0 to darken it. The room carries three of these,
/// reached through the pointers below.
typedef struct DryfieldNightGasStationSprtView {
    /* 0x00 */ byte pad_0[0x44];
    /* 0x44 */ u8   field_44;
    /* 0x45 */ byte pad_45[0x7];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[0x7];
    /* 0x54 */ u8   field_54;
} DryfieldNightGasStationSprtView;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationSprtView, 0x55);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at:
/// a room-sized block, far larger than the 0xC-byte `GpSprtRec` the table's
/// element type declares, so the room reaches its tail through a cast (as the
/// water tower's `DwtwSprtRec` and the dumping hole's `SprtBigRec` do). The
/// tail is a run of pointers to `DryfieldNightGasStationSprtView`; this room's
/// sit at 0xA0 / 0xAC / 0xC4.
typedef struct DryfieldNightGasStationSprtRec {
    /* 0x00 */ byte                             pad_0[0xA0];
    /* 0xA0 */ DryfieldNightGasStationSprtView* field_A0;
    /* 0xA4 */ byte                             pad_A4[0x8];
    /* 0xAC */ DryfieldNightGasStationSprtView* field_AC;
    /* 0xB0 */ byte                             pad_B0[0x14];
    /* 0xC4 */ DryfieldNightGasStationSprtView* field_C4;
} DryfieldNightGasStationSprtRec;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationSprtRec, 0xC8);

#endif // ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
