#ifndef ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
#define ROOMS_DRYFIELD_NIGHT_GAS_STATION_H

#include "common.h"

#include <psyq/libgte.h>

/// One of the per-view objects the room's sprite-table record points at. Its
/// flags sit eight bytes apart -- one `GpSprtCmd` record each -- in a run
/// starting at 0x34. `func_dryfield_night_gas_station_80180DC8` writes the
/// three at 0x44 / 0x4C / 0x54 together, to 1 to light the lamp and to 0 to
/// darken it; `func_dryfield_night_gas_station_80180C3C` writes the two at
/// 0x34 / 0x3C, to 1 or to 0 with its own argument. The room carries several
/// of these, reached through the pointers below.
typedef struct DryfieldNightGasStationSprtView {
    /* 0x00 */ byte pad_0[0x34];
    /* 0x34 */ u8   field_34;
    /* 0x35 */ byte pad_35[0x7];
    /* 0x3C */ u8   field_3C;
    /* 0x3D */ byte pad_3D[0x7];
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
/// tail is a run of pointers to `DryfieldNightGasStationSprtView`, twelve bytes
/// apart and starting at 0x34; `func_dryfield_night_gas_station_80180C3C`
/// reaches the 0x34 / 0x94 / 0xA0 / 0xAC / 0xC4 entries and
/// `func_dryfield_night_gas_station_80180DC8` the 0xA0 / 0xAC / 0xC4 ones.
typedef struct DryfieldNightGasStationSprtRec {
    /* 0x00 */ byte                             pad_0[0x34];
    /* 0x34 */ DryfieldNightGasStationSprtView* field_34;
    /* 0x38 */ byte                             pad_38[0x5C];
    /* 0x94 */ DryfieldNightGasStationSprtView* field_94;
    /* 0x98 */ byte                             pad_98[0x8];
    /* 0xA0 */ DryfieldNightGasStationSprtView* field_A0;
    /* 0xA4 */ byte                             pad_A4[0x8];
    /* 0xAC */ DryfieldNightGasStationSprtView* field_AC;
    /* 0xB0 */ byte                             pad_B0[0x14];
    /* 0xC4 */ DryfieldNightGasStationSprtView* field_C4;
} DryfieldNightGasStationSprtRec;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationSprtRec, 0xC8);

/// One entry of the room's vector lists: three coordinates plus padding, eight
/// bytes apart. Only the three coordinates are ever read or written.
typedef struct DryfieldNightGasStationVec {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 pad;
} DryfieldNightGasStationVec;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationVec, 0x8);

/// A 12-byte record copied whole, never read field by field.
typedef struct DryfieldNightGasStationBlob {
    /* 0x0 */ s8 b[12];
} DryfieldNightGasStationBlob;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationBlob, 0xC);

/// A table of pointers into layout data: a four-entry vector list, an
/// eight-entry vector list and four 12-byte records. The room keeps a template
/// and a live copy, and `func_dryfield_night_gas_station_8017FBD4` resets the
/// live lists from the template. Nothing pins where the table ends.
typedef struct DryfieldNightGasStationLayout {
    /* 0x0 */ s32                          field_0;
    /* 0x4 */ DryfieldNightGasStationVec*  field_4;
    /* 0x8 */ DryfieldNightGasStationVec*  field_8;
    /* 0xC */ DryfieldNightGasStationBlob* field_C;
} DryfieldNightGasStationLayout;

/// The block the room's effect task carries as its `spawnArg2`.
/// `func_dryfield_night_gas_station_80180E9C` keeps the spawn offset it hands
/// `Gp_SpawnEff` in `pos`, sets `active` once game flag nibble 0x63 has been
/// seen clear, and stores the per-anchor effect roll in `kind`. The bytes
/// around those fields are not reached here.
typedef struct DryfieldNightGasStationEffWork {
    byte    pad_0[0x10];
    SVECTOR pos;
    byte    pad_18[0xC];
    s16     active;
    s16     kind;
} DryfieldNightGasStationEffWork;

#endif // ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
