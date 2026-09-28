#ifndef GAMEPLAY_AREA_FLAGS_H
#define GAMEPLAY_AREA_FLAGS_H

#include "common.h"

#include "main/task.h"

/// 16-byte record of a room's `GpBit2List.field_0` list, ended by a `field_0`
/// of 0xFFFF. The same list serves two readers. The 2-bit bank code
/// (`Gp_ApplyBit2List` / `Gp_ApplyBit2Bank` / `Gp_LookupBit2Item`) keys on
/// `field_0`, publishes `field_2` as the item id to `Gp_PubItemLoc` and `field_6`
/// to `D_80114DDE`, and writes the low 2 bits of `field_6` into the bank. The
/// placement spawn (`Gp_SpawnAtPlace` / `Gp_SpawnPlaces` /
/// `Gp_SpawnPlaceById`) packs `field_0` and `field_4` into `GpEnemy.placeKey` as
/// `field_0 | (field_4 << 8)`, spawns the room's `GpEnemyDesc` whose id is
/// `field_2` and copies `field_2` to `GpEnemy.workType`; `field_8` / `field_A` / `field_C` are the world X/Y/Z
/// (`GpCoord.coord.t`) and `field_E` the yaw stored at coord +0x46 and passed to
/// `Gfx_RotMatrixY` when non-zero.
typedef struct _GpBit2Rec {
    /* 0x00 */ u16 field_0;
    /* 0x02 */ u16 field_2;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ u16 field_E;
} GpBit2Rec;
STATIC_ASSERT_SIZEOF(GpBit2Rec, 0x10);

/// Spawn header for `Gp_SpawnAtPlace` / `Gp_SpawnPlaces`. `field_0` is
/// `Task_SpawnFromTable` arg2 (0xFFFF terminator); `field_4` is the
/// `TaskDesc` table (`Gp_SpawnEnemyFromTable` uses idx 0).
typedef struct _GpEnemyDesc {
    /* 0x0 */ u16      field_0;
    /* 0x2 */ byte     pad_2[2];
    /* 0x4 */ TaskDesc field_4;
} GpEnemyDesc;
STATIC_ASSERT_SIZEOF(GpEnemyDesc, 0x10);

/// 8-byte list node walked by `Gp_ApplyBit2List` / `Gp_ApplyBit2Bank` /
/// `Gp_LookupBit2Item`. field_0 is a `GpBit2Rec` list (NULL skips;
/// `(GpBit2Rec*)-1` ends in `Gp_ApplyBit2List` / `Gp_ApplyBit2Bank`;
/// `(GpBit2Rec*)0x7FFFFFFF` ends in `Gp_LookupBit2Item`).
/// `Gp_SpawnPlaces` / `Gp_SpawnPlaceById` read field_4 as the room's
/// 0xFFFF-terminated `GpEnemyDesc` table.
/// `Gp_Bit2Banks[i].field_0` points at a table of these.
typedef struct _GpBit2List {
    /* 0x00 */ GpBit2Rec*   field_0;
    /* 0x04 */ GpEnemyDesc* field_4;
} GpBit2List;
STATIC_ASSERT_SIZEOF(GpBit2List, 0x8);

/// 4-byte record in 0xFF-terminated lists walked by `Gp_ApplyAreaRecs`.
/// `field_0` indexes `Gp_AreaTables` (same role as `GpAreaKey.stage`);
/// `field_1` indexes that table (same role as `GpAreaKey.area`);
/// `field_2` is the id written by `Gp_SetAreaObjId`. High nibble of `field_3`
/// is a `Mc_SaveData[0].gameMode` filter (0 = always, 0x10 if 0 or 2, 0x20 if
/// 1 or 3); low nibble nonzero sets `GpAreaObj.field_1` bit 2, else clears.
typedef struct _GpAreaApplyRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} GpAreaApplyRec;
STATIC_ASSERT_SIZEOF(GpAreaApplyRec, 4);

#endif // GAMEPLAY_AREA_FLAGS_H
