#ifndef GAMEPLAY_AREA_FLAGS_H
#define GAMEPLAY_AREA_FLAGS_H

#include "common.h"

#include "main/task_types.h"

/// `flagIndex` value that ends an `AreaObjectPlace` list.
enum { AREA_OBJECT_PLACE_END = 0xFFFF };

/// Low two bits of `AreaObjectPlace.state`, the slot's initial flag value.
enum { AREA_OBJECT_PLACE_STATE_MASK = 3 };

/// Bit 9 of `AreaObjectPlace.state`.
///
/// Published with the rest of `state`. The pickup prompt receives spawn
/// argument 1 when the bit is set and 0 when it is clear; the ordinary
/// pickup prompt collects the object immediately for argument 1.
enum { AREA_OBJECT_PLACE_PROMPT = 0x200 };

/// One object in a room's flag-bank list, ended by `AREA_OBJECT_PLACE_END`.
///
/// The record seeds that slot's packed 2-bit flag and, when `kind` matches
/// an enemy descriptor, supplies the spawn's place key, work type and
/// transform. `flagIndex` selects the flag — sixteen 2-bit values per word,
/// the low nibble the pair and the upper bits the word — and is stored in
/// the low half of the enemy place key. `placeKeyHigh` is shifted into that
/// key at bit 8, where the key keeps its stage and placement index; the
/// stored tables leave it zero. `kind` is a bank in the high byte and a
/// subtype in the low. Bank 0 is published as an item id, banks 0 and 1 open
/// the ordinary pickup prompt, and bank 8 is stored as the save point. The
/// spawn matches the whole word against the room's enemy-descriptor ids and
/// copies it to the enemy's work type. `state` contributes its low two bits
/// as the flag's initial value. Bit 8 of `state` is stored and published;
/// nothing reads it.
typedef struct {
    u16 flagIndex;    // Flag-bank index and place-key low half. AREA_OBJECT_PLACE_END ends the list
    u16 kind;         // Bank in the high byte, subtype in the low
    u16 placeKeyHigh; // Place key at bit 8 (stage, then placement index). Stored tables are zero
    u16 state;        // Bits 0..1 initial flag. Bit 9 is AREA_OBJECT_PLACE_PROMPT. Bit 8 is unread
    s16 x;            // World X, in game-coordinate units
    s16 y;            // World Y, in game-coordinate units
    s16 z;            // World Z, in game-coordinate units
    u16 yaw;          // Yaw, 0x1000 units per turn. Zero leaves the rotation unapplied
} AreaObjectPlace;
STATIC_ASSERT_SIZEOF(AreaObjectPlace, 0x10);

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
/// `Gp_LookupBit2Item`. The first word holds either an `AreaObjectPlace` list
/// (NULL skips) or an integer sentinel: -1 for `Gp_ApplyBit2List` /
/// `Gp_ApplyBit2Bank`, 0x7FFFFFFF for `Gp_LookupBit2Item`.
/// `Gp_SpawnPlaces` / `Gp_SpawnPlaceById` read field_4 as the room's
/// 0xFFFF-terminated `GpEnemyDesc` table.
/// `Gp_Bit2Banks[i].field_0` points at a table of these.
typedef struct _GpBit2List {
    /* 0x00 */ union {
        AreaObjectPlace* records;
        s32              sentinel;
    } field_0;
    /* 0x04 */ GpEnemyDesc* field_4;
} GpBit2List;
STATIC_ASSERT_SIZEOF(GpBit2List, 0x8);

/// 4-byte record in 0xFF-terminated lists walked by `Gp_ApplyAreaRecs`.
/// `field_0` indexes `Gp_AreaTables` (same role as `GameLocationKey.stage`);
/// `field_1` indexes that table (same role as `GameLocationKey.area`);
/// `field_2` is the id written by `areaSetPlacementVariant`. High nibble of `field_3`
/// is a `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode` filter (0 = always, 0x10 if 0 or 2, 0x20 if
/// 1 or 3); low nibble nonzero sets `GpAreaObj.spawnFlags` bit 2, else clears.
typedef struct _GpAreaApplyRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} GpAreaApplyRec;
STATIC_ASSERT_SIZEOF(GpAreaApplyRec, 4);

#endif // GAMEPLAY_AREA_FLAGS_H
