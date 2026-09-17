#ifndef ROOMS_NEO_ARK_POWER_PLANT_1_H
#define ROOMS_NEO_ARK_POWER_PLANT_1_H

#include "common.h"

/// One of the two per-view objects the room's sprite-table record points at.
/// Its byte at 0xC is the skip-OT-link flag the sibling rooms' sprite views
/// carry too (`DwtSprtView.field_C`, `DwtwSprtViewState.field_C`,
/// `MineForkedTunnelViewA.field_1C` / `ViewB.field_2C`): non-zero leaves the
/// view's sprites out of the ordering table, zero draws them.
typedef struct NeoArkPowerPlant1SprtView {
    /* 0x00 */ byte pad_0[0xC];
    /* 0x0C */ u8   field_C;
} NeoArkPowerPlant1SprtView;
STATIC_ASSERT_SIZEOF(NeoArkPowerPlant1SprtView, 0xD);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at: a
/// room-sized block, far larger than the 0xC-byte `GpSprtRec` the table's
/// element type declares, so the room reaches its tail through a cast (as the
/// cavern's `MineCavernSprtRec` and the water tank's `DwtSprtRec` do). Two of
/// the per-view pointers in that tail sit at 0x40 and 0x4C, and both name a
/// `NeoArkPowerPlant1SprtView`.
typedef struct NeoArkPowerPlant1SprtRec {
    /* 0x00 */ byte                       pad_0[0x40];
    /* 0x40 */ NeoArkPowerPlant1SprtView* field_40;
    /* 0x44 */ byte                       pad_44[0x8];
    /* 0x4C */ NeoArkPowerPlant1SprtView* field_4C;
} NeoArkPowerPlant1SprtRec;
STATIC_ASSERT_SIZEOF(NeoArkPowerPlant1SprtRec, 0x50);

#endif
