#ifndef MAIN_MC_TYPES_H
#define MAIN_MC_TYPES_H

#include <psyq/sys/types.h>

#include "common.h"

#include "main/session_types.h"

/// Empty-item marker and attachment states stored in an inventory row.
enum {
    INVENTORY_ITEM_NONE                 = 0,
    INVENTORY_ATTACHMENT_NONE           = 0,
    INVENTORY_ATTACHMENT_EQUIPPED_ARMOR = -1,
};

/// One inventory row holding an item, its armour attachment position and quantity.
///
/// `InventoryItemRange` selects ranges in arrays of these four-byte rows.
/// `itemId == INVENTORY_ITEM_NONE` alone marks a free row; the other fields may
/// retain old values. Positive `attachSlot` values are one-based positions on
/// the equipped armour (up to its attachment capacity, capped at ten).
/// The equipped weapon is selected separately, so a zero slot does not prove
/// that an item is unequipped.
///
/// Ammunition ids 0xA0..0xBF share a quantity stack per item id within a range.
/// Their quantity includes rounds loaded into weapons; subtract the loaded
/// quantities to obtain the available amount. Other items occupy separate
/// rows, normally with quantity one. Row pointers borrow the selected table's
/// storage; sorting and transfers can replace the item at the same address.
typedef struct {
    u8  itemId;     // Item id (0 free); stored ids are limited to one byte
    s8  attachSlot; // Armour attachment position (0 none, 1..10 attached, -1 equipped armour)
    u16 qty;        // Total item units, including ammunition loaded into weapons
} InventoryItemRow;
STATIC_ASSERT_SIZEOF(InventoryItemRow, 0x4);

/// Weapon-item id base and marker for a weapon without a secondary consumable slot.
enum {
    EQUIPMENT_WEAPON_ITEM_FIRST            = 0x80,
    EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE = 0xFF,
};

/// Saved consumable selections and remaining loads for one weapon item id.
///
/// The primary and secondary firing modes each select an item and track its
/// loaded rounds or rechargeable supply units. Item ids are 0xA0..0xBF, with
/// `INVENTORY_ITEM_NONE` for no selection; the secondary slot additionally uses
/// `EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE` when that weapon has no such slot.
/// A zero quantity can retain the selected item id for the next reload.
/// Ordinary ammunition loads remain included in the inventory row's total
/// quantity. Built-in supplies instead retain their charge in this record.
///
/// `McSaveState::weaponItems` has one record for each id 0x80..0x9F, indexed
/// by item id minus `EQUIPMENT_WEAPON_ITEM_FIRST`, independently of inventory
/// row position or which weapon is equipped. Pointers borrow saved-state
/// storage; loading or resetting that state can replace their contents.
typedef struct {
    u8  primaryItemId;   // Primary consumable item id (0 none, otherwise 0xA0..0xBF)
    u8  primaryQty;      // Primary rounds or supply units remaining, limited by the weapon's capacity
    u8  secondaryItemId; // Secondary consumable item id (0 none, 0xA0..0xBF selected, 0xFF unavailable)
    u8  secondaryQty;    // Secondary rounds or supply units remaining, limited by the weapon's capacity
    s32 field_4;         // Role and interpretation unproven: only full-word zero writes observed
} EquipmentWeaponLoad;
STATIC_ASSERT_SIZEOF(EquipmentWeaponLoad, 0x8);

/// Row storage selectors for `InventoryItemRange::tableId`.
enum {
    INVENTORY_ITEM_TABLE_SAVED       = 0, // `Mc_SaveData[0].state.itemRows`
    INVENTORY_ITEM_TABLE_INDIRECT    = 1, // Rows addressed by `Gp_ItemTable1`; backing and lifetime unproven
    INVENTORY_ITEM_TABLE_AREA_GRANTS = 2, // `Gp_ItemTable2`, holding items granted on area entry
};

/// Largest row count an `InventoryItemRange` can represent; a literal count, not a sentinel.
enum { INVENTORY_ITEM_RANGE_MAX_ROWS = 0xFF };

/// A contiguous range of item rows used by inventory operations.
///
/// `firstRow` and `rowCount` count `InventoryItemRow` elements in the selected table.
/// The range includes free rows: `rowCount` is capacity, not occupied-item count.
/// `firstRow + rowCount` must fit the backing table; zero rows denotes an empty
/// range. A range starting at row 0 with 255 rows excludes row 255.
/// The descriptor owns no storage. Its copies refer to the same table, which
/// must remain available while the range is used.
typedef struct {
    u8 firstRow; // Absolute index of the first row in the selected table
    u8 rowCount; // Number of rows, including free rows (0..255)
    u8 tableId;  // Storage selector (0/default saved, 1 indirect table, 2 area grants)
    u8 field_3;  // Role unproven: zero in initializers, cleared and copied with the range
} InventoryItemRange;
STATIC_ASSERT_SIZEOF(InventoryItemRange, 0x4);

/// Saved root transform and actor-specific resume state for an area placement.
///
/// `McSaveState::enemyPoses` holds 32 records. In saved-pose restoration mode,
/// only placements with a matching key are spawned. Saving an existing key
/// keeps its first record; a full table evicts a record outside the saved
/// location's stage/area, or its final record if none qualifies.
///
/// Positions retain the low 16 bits of the model root's translation in its
/// parent's frame, restored with sign extension. Each Euler angle retains the
/// high byte of a signed 16-bit angle (4096 units per turn), discarding its low
/// eight bits; restoration shifts the byte left eight bits into a 16-bit angle.
///
/// `resumeState == 0` marks a free slot; occupied slots preserve the actor's
/// nonzero spawn-state byte. Key lookups do not test occupancy. Area-layout
/// resets remove the area's records and clear vacated keys to zero. Pointers
/// borrow saved-state storage; insertion, resets and loading can replace the
/// record at the same address.
typedef struct {
    u8  pitch;       // X Euler angle's high byte; one step is 256 angle units
    u8  yaw;         // Y Euler angle's high byte; one step is 256 angle units
    u8  roll;        // Z Euler angle's high byte; one step is 256 angle units
    s8  resumeState; // Actor-defined spawn-state byte (0 free, nonzero occupied)
    s16 x;           // Model-root X translation in signed game-coordinate units
    s16 y;           // Model-root Y translation in signed game-coordinate units
    s16 z;           // Model-root Z translation in signed game-coordinate units
    u16 placeKey;    // Placement key: bits 12..15 index, 8..11 stage, 0..7 area; excludes layout variant
} AreaSavedEnemyPose;
STATIC_ASSERT_SIZEOF(AreaSavedEnemyPose, 0xC);

/// The save data: everything a memory card save holds, resident in main's BSS
/// and copied out as one image.
///
/// It carries the run's progress - where the player was, the time played, the
/// player's statistics and the whole inventory with its per-weapon equipment -
/// and the record of what the game has already done: which stages and areas
/// were visited, which placed enemies were taken out of the world and where
/// they were left, which items have been seen, and what the shops still hold in
/// stock. The header block and the data block each carry a checksum pair.
typedef struct {
    byte                unknown_0[0x4];
    GameLoc             at4;               // Place the save was made at, as `GameSession.location`
    u16                 playTime;          // Played time in minutes (capped at 0xEA5F)
    s8                  clearCount;        // Times the game has been completed (0 never, capped at 99)
    s8                  gameMode;          // Mode the run is played in (0..2); the stat, cost and item-grant tables have one variant per mode
    u8                  visitFlags;        // One bit per stage marking it as visited; bit 0 marks the new-game setup as done
    s8                  saveNumber;        // Number of this save among those made at the same save point (1..99)
    u8                  savePoint;         // Save point the save was made at, indexing the tables of place names the slot and the save header print (1..16)
    s8                  companionType;     // Companion the save carries (0 none, otherwise 1..3 index `Gp_AllyIdBase`)
    s32                 playerExp;         // Player experience as of the save, as `Player_Status.exp`
    s32                 playerBp;          // Player BP as of the save, as `Player_Status.bp`
    u16                 hdrChecksum;       // Sum over the header's 0x38 bytes, which start at `at4`
    u16                 hdrChecksumInv;    // Complement of `hdrChecksum`, written with it
    byte                unknown_20[0x1];
    s8                  vibration;         // Vibration setting (0 on, 1 off)
    s8                  characterId;       // Character the save plays as (1-based); the weapon, animation and placement tables are indexed by it
    s8                  demoScene;         // Attract demo being played back (0 during normal play)
    byte                unknown_24[0x1];
    u8                  moveMode;          // Default movement (0 walk, 1 run)
    u8                  hpBonus;           // Addend to the player's maximum HP
    u8                  mpBonus;           // Addend to the player's maximum MP
    AreaSavedEnemyPose  enemyPoses[0x20];  // The placed enemies taken out of the world, filed by their placement key
    s8                  buttonLayout;      // Button layout the pad is remapped through (0..2)
    s8                  soundMode;         // Sound output (0 stereo, 1 mono)
    s8                  musicVolume;       // Music volume (0..3, 3 is off)
    s8                  cursorMode;        // Cursor behaviour in the menus (0 remembers the row, 1 resets it)
    InventoryItemRow    itemRows[0x100];   // The save's own item table, indexed by row; the rows the player carries are `carriedItems`
    s32                 collectedBits[4];  // 128 bits, one per collectible the player has picked up
    InventoryItemRange  carriedItems;      // Window on the player's rows of `itemRows`
    u8                  unknown_5C0;
    u8                  field_5C1;         // Role unproven: while it reads 1 the actors stop moving and stop pushing each other apart
    s8                  cheatMode;         // While set nothing is spent - no MP for the abilities, no ammunition and no attachments - and the save is left out of the save-slot comparison
    s8                  interlace;         // Non-zero runs the display interlaced
    byte                unknown_5C4;
    s8                  sceneEvent;        // Scene event a script arms for the music and sound task
    byte                unknown_5C6[0x1];
    s8                  companionVariant;  // Which variant of `companionType` is spawned
    EquipmentWeaponLoad weaponItems[0x20]; // Primary and secondary consumable loads, indexed by weapon item id - 0x80
    s16                 companionHp;       // Companion's current HP
    s16                 companionHpMax;    // Companion's maximum HP
    u16                 field_6CC;
    u16                 field_6CE;
    s32                 itemSeenBits[0x60];    // One bit per item id (ids at or above 0x180 count as seen), set once the item was looked at
    u8                  attachLevels[0x12];    // Level bought for each Parasite Energy slot (0..3), addressed as page * 3 + column
    s16                 attachUseCounts[19];   // Times each Parasite Energy slot was used; the Play Data panel reports the first twelve
    s32                 weaponUseCounts[0x20]; // Times each weapon was used, indexed by item id - 0x80
    s8                  itemLevelBonus[0x20];  // Addend to the level of items 0x60-0x7F
    byte                unknown_928[0x1];
    s8                  field_929;
    s8                  replayRank; // Rank the replay earns (0 none, 1, 2)
    u8                  saveCount;  // How many times the game has been saved (capped at 99)
    s32                 field_92C;
    s32                 field_930;
    s32                 shopTiers;         // Bitmask of the 13 price tiers the parking-lot shop offers
    u32                 shopStock;         // 12 two-bit stock levels for that shop
    u16                 dataChecksum;      // Sum over the save's data block
    u16                 dataChecksumInv;   // Complement of `dataChecksum`, written with it
    u16                 bufferChecksum;    // Sum over the first byte of each memcard buffer slot
    u16                 bufferChecksumInv; // Complement of `bufferChecksum`, written with it
} McSaveState;

/// First 128 bytes of a saved-state block, cached for a memory-card slot.
///
/// Read from byte offset 0x200 in the card file. The displayed values mirror
/// the corresponding fields of `McSaveState`; this sector is not a full state.
/// `headerChecksum` covers 56 signed bytes beginning at `location`, including
/// the header checksum pair and the first 28 bytes of `remainingBytes`.
/// Both sums retain their low 16 bits. The full-block `saveChecksum` cannot
/// be verified from this prefix alone.
typedef struct {
    s16     saveChecksum;             // Sum of the 0x940 signed payload bytes in the full saved-state block
    s16     saveChecksumComplement;   // Ones' complement of `saveChecksum`
    GameLoc location;                 // Saved world location
    u16     playTime;                 // Minutes played (0..59999)
    s8      clearCount;               // Completed runs (0 never completed, 1..99 completed)
    s8      gameMode;                 // Run mode (0 normal/replay, 1 Bounty, 2 Scavenger, 3 Nightmare)
    u8      visitFlags;               // Bit 0: new-game setup done; bits 1..5: visited stages
    s8      saveNumber;               // Save label number at this save point (1..99), distinct from total saves
    u8      savePoint;                // Place-label index (1..16); 15 is Opening and hides mode/EXP/BP in the preview
    s8      companionType;            // Saved companion family (0 none, 1..3 companion)
    s32     playerExp;                // Player experience captured for the save
    s32     playerBp;                 // Player BP captured for the save
    u16     headerChecksum;           // Sum of the 56 signed bytes beginning at `location`
    u16     headerChecksumComplement; // Ones' complement of `headerChecksum`
    u8      remainingBytes[0x60];     // Rest of the sector; the preview retains its representation without interpreting it
} McSavePreview;
STATIC_ASSERT_SIZEOF(McSavePreview, 0x80);
STATIC_ASSERT(OFFSET_OF(McSavePreview, location) == 4, McSavePreview_location);
STATIC_ASSERT(OFFSET_OF(McSavePreview, remainingBytes) == 0x20, McSavePreview_remainingBytes);

/// Full save image and its bounded cached-preview representation.
/// Both records have the same initial fields through the checksum pair.
typedef union {
    McSaveState   state;
    McSavePreview preview;
} McSaveData;
STATIC_ASSERT_SIZEOF(McSaveData, 0x944);
STATIC_ASSERT(OFFSET_OF(McSaveData, state.at4) == 4, McSaveData_at4);
STATIC_ASSERT(OFFSET_OF(McSaveData, state.playTime) == 0xC, McSaveData_playTime);

#endif // MAIN_MC_TYPES_H
