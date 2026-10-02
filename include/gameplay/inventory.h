#ifndef GAMEPLAY_INVENTORY_H
#define GAMEPLAY_INVENTORY_H

#include "common.h"

#include "gameplay/starter_inventory.h"

/// 4-byte table entry in `Gp_ItemMaps` (8 entries). field_1 is an item id
/// used to index `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems`; field_0 selects which (id, count) pair;
/// field_2 is the mapped item id (`Gp_ApplyItemMap`).
typedef struct _GpItemMap {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemMap;
STATIC_ASSERT_SIZEOF(GpItemMap, 0x4);

/// 4-byte entry in `Gp_RelatedQty1` / `Gp_RelatedQty0` (32 entries, item ids
/// 0x80–0x9F). field_0 is a count (`Gp_GetRelatedQty` / `Gp_ApplyItemMap`);
/// `related` holds the item ids the weapon accepts, searched in order
/// (`Gp_BuildAttachList` / `Gp_NthRelatedId` / `Gp_NthStockRelated` /
/// `Gp_EquipRelatedBank` / `Gp_EquipRelatedItem`).
/// `Gp_RelatedQty0` describes `EquipmentWeaponLoad::primaryItemId` and
/// `primaryQty` (arg0 == 0); `Gp_RelatedQty1` describes `secondaryItemId` and
/// `secondaryQty`.
/// `Gp_QtyById0` / `Gp_QtyById1` are the same tables indexed by raw item id
/// (`Gp_RelatedQty0` is `Gp_QtyById0 + 0x200`).
typedef struct _GpItemQty {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 related[3];
} GpItemQty;
STATIC_ASSERT_SIZEOF(GpItemQty, 0x4);

/// The 32 weapon entries, also read as packed bytes by the ammo-row scan.
typedef union GpRelatedItemTable {
    GpItemQty rows[32];
    u8        bytes[32 * sizeof(GpItemQty)];
} GpRelatedItemTable;
STATIC_ASSERT_SIZEOF(GpRelatedItemTable, 0x80);

/// 4-byte row in `Gp_StackLimits`, indexed by item id − 0xA0 (ids ≥ 0xA0).
/// perBuy is the default pickup/purchase count; maxHeld is the quantity limit.
typedef struct _GpItemA0 {
    /* 0x00 */ u8  perBuy;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u16 maxHeld;
} GpItemA0;
STATIC_ASSERT_SIZEOF(GpItemA0, 0x4);

/// The starting max HP at the head of a `Gp_StatRow`. `Gp_RecalcMaxHp` reads it
/// as the unsigned halfword it writes straight into `gPlayerStatus.hpMax`,
/// while the Mist shooting gallery's STATUS panel reads the same slot as a full
/// word, so the slot is declared both ways.
typedef union _GpStatBase {
    /* 0x0 */ u16 half;
    /* 0x0 */ s32 word;
} GpStatBase;
STATIC_ASSERT_SIZEOF(GpStatBase, 0x4);

/// 8-byte row in `Gp_StatRows` (4 entries), indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode`.
/// base is the starting max HP (see `GpStatBase`). field_4 is the word added
/// into `gPlayerStatus.mpMax` (`Gp_RecalcMaxMp`).
typedef struct _GpStatRow {
    /* 0x00 */ GpStatBase base;
    /* 0x04 */ s32        field_4;
} GpStatRow;
STATIC_ASSERT_SIZEOF(GpStatRow, 0x8);

/// Object at `Task::spawnArg2` for `Gp_BindItemObj2` / `Gp_PublishItemObj` /
/// `Gp_PickupResultTask` / `Gp_WaitItemFlag2`. `field_8` is the packed item id passed
/// to `Gp_GetCurBit2Flag` (and inlined by `Gp_WaitItemFlag2`).
/// `field_A` is the item/location halfword copied into `Gp_PubItemLoc` by
/// `Gp_PublishItemObj` and cleared by `Gp_PickupResultTask` on the cancel path.
typedef struct _GpItemObj8 {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u8   field_8;
    /* 0x09 */ byte pad_9;
    /* 0x0A */ u16  field_A;
} GpItemObj8;

/// Low-nibble item subtypes used by inventory icons and ammunition labels.
///
/// The subtype's meaning depends on the catalogue category: medicine uses 1,
/// while ammunition uses the caliber or load kind below.
enum {
    ITEM_SUBTYPE_MASK     = 0x0F,
    ITEM_SUBTYPE_MEDICINE = 1,
    ITEM_AMMO_9MM         = 1,
    ITEM_AMMO_44_MAGNUM   = 3,
    ITEM_AMMO_40MM        = 4,
    ITEM_AMMO_12_GAUGE    = 5,
    ITEM_AMMO_556MM       = 6,
    ITEM_AMMO_BATTERY     = 8
};

/// Restrictions in `ItemDesc::flags`; these masks leave the stored byte intact.
enum {
    ITEM_FLAG_NO_DISCARD    = 0x01, // Also blocks transfers in the battle-loot menu.
    ITEM_FLAG_NO_ATTACHMENT = 0x04  // Cannot occupy an armor attachment slot.
};

/// Catalogue properties and identified/unidentified text for an inventory item.
///
/// `Gp_ItemDescs` and `Gp_KeyItemDescs` use this same row layout. The high
/// nibble of `classification` groups entries (0 other, 1 armor, 2 weapon,
/// 3 ammunition, 4 key item); the low nibble is a category-specific subtype.
/// `textFields` borrows text for the gameplay image's lifetime: an identified
/// name and two description lines, followed by their unidentified counterparts.
/// Fields end at NUL, newline or a `\n` / `\N` escape. An empty unidentified
/// name makes the item identified from the start.
typedef struct {
    u16       price;          // Purchase price in BP; replay-bonus credit is half.
    u8        classification; // Packed catalogue category and subtype.
    u8        flags;          // ITEM_FLAG_* restrictions; the role of 0x08 is unproven.
    const u8* textFields;     // Six delimited text fields, owned by the gameplay image.
} ItemDesc;
STATIC_ASSERT_SIZEOF(ItemDesc, 0x8);

/// Scan dest used while `Gp_InitStarterInv` copies the current inventory out.
#define D_8010D55C Gp_ScanPtrs[3]

#endif // GAMEPLAY_INVENTORY_H
