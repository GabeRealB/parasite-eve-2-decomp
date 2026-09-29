#ifndef GAMEPLAY_PRIVATE_AREA_FLAGS_H
#define GAMEPLAY_PRIVATE_AREA_FLAGS_H

#include "common.h"

#include "gameplay/area_flags.h"

/// 8-byte entry in `Gp_Bit2Banks`, indexed by session field_7 /
/// `GameLocationKey.stage` / `Mc_SaveData[0].state.at4.loc.stage`. field_0 is a
/// `GpBit2List` table applied by `Gp_ApplyBit2Bank` / `Gp_ApplyBit2List`.
/// field_4 is packed 2-bit flags (`Gp_GetBit2Flag` / `Gp_SetBit2Flag` /
/// `Gp_GetCurBit2Flag` / `Gp_SetCurBit2Flag` / `Gp_SpawnPlaceById`).
typedef struct _GpBit2Bank {
    /* 0x00 */ GpBit2List* field_0;
    /* 0x04 */ u32*        field_4;
} GpBit2Bank;
STATIC_ASSERT_SIZEOF(GpBit2Bank, 0x8);

#endif // GAMEPLAY_PRIVATE_AREA_FLAGS_H
